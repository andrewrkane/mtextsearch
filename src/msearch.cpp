// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath> // for log()
#include <algorithm>
#include <chrono>
#include <sys/stat.h>

#include "mtokenizer.hpp"
#include "mdictionary.hpp"

/* read in mindex file, run queries from stdin, output DOCNO results to stdout */

struct Scored { int docid; float score; Scored(int d,float s) {docid=d; score=s;} }; Scored EmptyScore(-1,-1);
inline bool mincomp(const Scored& a, const Scored& b) { return a.score>b.score; } // score order
class TopkHeap : public std::vector<Scored> { public:
  TopkHeap(int topk=5) : std::vector<Scored>(topk,EmptyScore) {}
  inline bool add(int docid, float score) { if (score>front().score) { pop_heap(begin(),end(),mincomp); pop_back(); push_back(Scored(docid,score)); push_heap(begin(),end(),mincomp); return true; } return false; }
  void done() { sort(begin(),end(),mincomp); }
  //void dump() { for (int i=0;i<size();i++) {std::cerr<<(*this)[i].docid<<":"<<(*this)[i].score<<" ";} std::cerr<<std::endl; }
};

class PLIter { byte* d; byte* dend; public: int32_t id; int32_t freq; int plsize; float w;
  PLIter() {std::cerr<<"ERROR: PLIter()"<<std::endl; exit(-1);}
  PLIter(byte* data, int blen, float weight) { d=data; dend=d+blen; id=freq=0; plsize=readVByte(d); w=weight; int lastid=(plsize>1?readVByte(d):-1); next(); }
  inline bool next() { if (d>=dend) return false; id+=readVByte(d); freq=readVByte(d); return true; }
};
inline bool PLICompID(const PLIter* i, const PLIter* j) { return i->id < j->id; }
class PLIV : public std::vector<PLIter> { protected: std::vector<byte*> datav; public:
  PLIter& add(byte* data, int blen, float weight) { datav.push_back(data); push_back(PLIter(data,blen,weight)); return (*this)[size()-1]; } //this vector owns data array
  virtual ~PLIV() { for (int i=0;i<datav.size();++i) { delete datav[i]; } datav.resize(0); resize(0); }
};

class MSearch { public: bool bMath; float alpha; protected: int k;
  DocnamesTwoLayer* docs; uint64_t totaltokens; //docs(docname->docsize)
  std::ifstream* postfile; DictionaryTwoLayer* dict; //dict(token->location) points into postfile
  MTokenizer tokenizer;

  inline void getIterators(/*in*/MTokenizer::TokenList& tokens, /*out*/PLIV& listIters) {
    tokens.sort();
    for (int i=0;i<tokens.size();) {
      cchar* token=tokens[i]; int count=1; i++;
      while (i<tokens.size() && strcmp(token,tokens[i])==0) { ++count; ++i; }
      uint64_t loc=dict->getV(token);
      if (loc==IntDeltaV::UNKNOWN) continue; //not-in-data
      std::ifstream& in=*postfile; in.clear(); in.seekg(loc);
      std::string t; in>>t; if (strcmp(token,t.c_str())!=0) {std::cerr<<"ERROR: pointing to wrong token "<<token<<" -> "<<t<<std::endl; exit(-1);}
      int blen; in>>blen;
      { std::string line; getline(in,line); if (line.compare("")!=0) {std::cerr<<"ERROR: index extra postings info "<<token<<std::endl; exit(-1);} }
      byte* data=new byte[blen];
      in.read((char*)data,blen);
      { std::string line; getline(in,line); if (line.compare("")!=0) {std::cerr<<"ERROR: index extra postings "<<token<<std::endl; exit(-1);} }
      float weight=count/(count+10.0f); if (bMath) { weight*=(token[0]=='#'?alpha:1.0f-alpha); }
      PLIter& pli=listIters.add(data,blen,weight);
      if (pli.plsize>docs->size()) {std::cerr<<"ERROR: plsize "<<pli.plsize<<" > docs.size "<<docs->size()<<std::endl; exit(-1);}
      //std::cerr<<"Found \'"<<token<<"\' count="<<count<<" plsize="<<pli->plsize<<" bytelength="<<blen<<std::endl;
    }
  }

  void doQuery(/*in*/const std::string& prefix, /*in*/PLIV& listIters, std::ostream& out, int doccount, float avgDocSize) {
    // precompute IDF for BM25
    for (int i=0;i<listIters.size();i++) { PLIter& pli=listIters[i];
      pli.w*=log(1.0f+((float)doccount-pli.plsize+0.5f)/(pli.plsize+0.5f));
      //std::cerr<<"idf*weight="<<pli.w<<std::endl;
    }
    // intersect iterators w scoring
    TopkHeap h(k); float T=0.0f;
    std::vector<PLIter*> X; for (int i=0;i<listIters.size();i++) X.push_back(&listIters[i]);
    while (X.size()>0) {
      SORT_ITERS:
      sort(X.begin(), X.end(), PLICompID);
      //for (int i=0;i<X.size();i++) {std::cerr<<X[i]->id<<" ";} std::cerr<<std::endl;
      // pivot from threshold
      int Pi=0; float Smax=0.0f; for (; Pi<X.size(); Pi++) {Smax+=X[Pi]->w*(1.2f+1.0f); if (Smax>T) break; }
      if (Pi>=X.size()) break; //done
      int Pid=X[Pi]->id;
      // advance to pivot
      if (Pi!=0 && X[0]->id != Pid) {
        for (int i=0; i<Pi; i++) { PLIter& pli=*X[i];
          while (pli.id<Pid) { if (!pli.next()) { X.erase(X.begin()+i); i--; Pi--; break; } } //skip
        }
        goto SORT_ITERS;
      }
      // add other iterators at Pid
      for (; Pi<X.size(); Pi++) { if (Pi+1>=X.size() || X[Pi+1]->id!=Pid) break; Smax+=X[Pi+1]->w*(1.2f+1.0f); }
      // score iterators at docid (early termination)
      int docid; float score=0.0f;
      for (int i=0; i<=Pi; i++) {
        PLIter& pli=*X[i]; if (i==0) docid=pli.id; else if (pli.id!=docid) break;
        // BM25 see https://en.wikipedia.org/wiki/Okapi_BM25
        float tf=pli.freq*(1.2f+1.0f) / (pli.freq + 1.2f*(1.0f - 0.75f + 0.75f*docs->getV(docid)/avgDocSize));
        //std::cerr<<"pli.freq="<<pli.freq<<" doclength="<<docs->getV(docid)<<" avgDocSize="<<avgDocSize<<std::endl;
        //std::cerr<<"tf="<<tf<<" tf*w="<<tf*pli.w<<std::endl;
        score += tf*pli.w; Smax -= pli.w*(1.2f+1.0f);
        if ((score+Smax)<=T) {
          for (; i<=Pi; i++) { if (!X[i]->next()) { X.erase(X.begin()+i); i--; Pi--; } }
          goto SORT_ITERS;
        }
        // advance iterators at docid
        if (!pli.next()) { X.erase(X.begin()+i); i--; Pi--; }
      }
      if (h.add(docid,score)) { T=h.front().score; }
    }
    h.done();
    // output
    for (int i=0;i<h.size()&&h[i].docid>=0;i++) { char c[1<<10]; docs->getK(h[i].docid,c,1<<10); out<<prefix<<c<<"\t"<<i+1<<"\t"<<h[i].score<<std::endl; }
  }

public:
  MSearch() { bMath=false; alpha=0.18f; docs=NULL; totaltokens=0; postfile=NULL; dict=NULL; k=10; }
  virtual ~MSearch() { if (docs!=NULL) delete docs; docs=NULL;
    if (dict!=NULL) delete dict; dict=NULL;
    if (postfile!=NULL) postfile->close(); postfile=NULL; }
  void setk(int t) { if (t<=0) {std::cerr<<"ERROR: invalid k="<<t<<std::endl;exit(-1);} k=t; }
  void setAlpha(float a) { if (a<0||a>1) {std::cerr<<"ERROR: invalid alpha "<<a<<std::endl; exit(-1);} alpha=a; }

  void query(std::string query) {
    std::chrono::high_resolution_clock::time_point s=std::chrono::high_resolution_clock::now();
    // named vs normal
    std::string prefix="", qname=""; size_t cut=query.find(';');
    if (cut!=std::string::npos) { qname=query.substr(0,cut); prefix=qname+"\t"; query=query.substr(cut+1); }
    std::cerr<<"query: "<<(true&&cut!=std::string::npos?qname:query)<<std::endl;
    // split into tokens
    MTokenizer::TokenList tokens; tokenizer.process(query.c_str(),query.length(),tokens);
    if (tokens.size()<=0) {std::cerr<<"empty query"<<std::endl; return;}
    //{ ofstream out("queries-processed.txt",std::ios_base::app); out<<qname<<";"; for (int i=0;i<tokens.size();i++) { out<<" "<<tokens[i]; } out<<std::endl; return; }
    //{ for (int i=0;i<tokens.size();i++) { std::cout<<" "<<tokens[i]; } std::cout<<std::endl; return; }
    //std::cerr<<"found "<<tokens.size()<<" tokens"<<std::endl;
    // stats
    int doccount=docs->size(); float avgDocSize=(double)totaltokens/doccount;
    //std::cerr<<"avgDocSize="<<avgDocSize<<std::endl;
    // find postings lists and query
    PLIV listIters; getIterators(tokens, listIters);
    //std::cerr<<"found "<<listIters.size()<<" lists"<<std::endl;
    doQuery(prefix, listIters, std::cout, doccount, avgDocSize);
    std::chrono::high_resolution_clock::time_point e=std::chrono::high_resolution_clock::now();
    std::cerr<<"Query took "<<(double)std::chrono::duration_cast<std::chrono::microseconds>(e-s).count()/1000<<"ms"<<std::endl;
  }

  void input(const char* fn) {
    //std::chrono::high_resolution_clock::time_point s=std::chrono::high_resolution_clock::now();
    // postfile
    if (postfile!=NULL) {std::cerr<<"ERROR: only supporting one index file"<<std::endl; exit(-1);}
    postfile=new std::ifstream(fn); std::string line; //std::cerr<<"Input "<<fn<<std::endl;
    std::ifstream& in=*postfile; if (!in.is_open()) {std::cerr<<"ERROR: Could not open input file "<<fn<<std::endl; exit(-1);}
    getline(in,line); if (!in) {std::cerr<<"ERROR: Empty input file "<<fn<<std::endl; exit(-1);}
    if (!(bMath && line.compare("math.mindex.1")==0) && line.compare("text.mindex.1")!=0) {std::cerr<<"ERROR: Unknown file format "<<fn<<" "<<line<<std::endl; exit(-1);} // external math tokenizer goes to text.mindex.1
    // meta
    std::string metafn=(std::string)fn+".meta"; std::ifstream metain(metafn); if (!metain) {std::cerr<<"ERROR: loading meta file "<<metafn<<std::endl; exit(-1);}
    struct stat sb; int er=stat(fn,&sb); uint64_t fsize=(uint64_t)sb.st_size; uint64_t t; metain>>t; if (er==-1 || t!=fsize) {std::cerr<<"ERROR: meta "<<metafn<<" wrong size match for "<<fn<<std::endl; exit(-1);}
    getline(metain,line); if (line.compare("")!=0) {std::cerr<<"ERROR: meta "<<metafn<<" extra size match info "<<line<<std::endl; exit(-1);}
    docs=new DocnamesTwoLayer(metain,metafn.c_str()); metain>>totaltokens; getline(metain,line); if (line.compare("")!=0) {std::cerr<<"ERROR: meta "<<metafn<<" extra totaltokens "<<line<<std::endl; exit(-1);}
    dict=new DictionaryTwoLayer(metain,metafn.c_str()); metain.close();
    //std::chrono::high_resolution_clock::time_point e=std::chrono::high_resolution_clock::now();
    //std::cerr<<"Input "<<metafn<<" took "<<(double)std::chrono::duration_cast<std::chrono::microseconds>(e-s).count()/1000 <<"ms"<<" (docs="<<docs->size()<<",tt="<<totaltokens<<",terms="<<dict->size()<<")"<<std::endl;
  }

  void dumpDictionary() {
    std::ostream& out=std::cout;
    for (int i=0;i<dict->size();i++) {
      uint64_t loc=dict->getV(i); std::ifstream& in=*postfile; in.clear(); in.seekg(loc);
      std::string t; in>>t; int blen; in>>blen; //only non-math
      { std::string line; getline(in,line); if (line.compare("")!=0) {std::cerr<<"ERROR: index extra postings "<<t<<std::endl; exit(-1);} }
      byte data[10]; byte* d=data; in.read((char*)data,std::min(10,blen)); int plsize=readVByte(d);
      if (plsize>docs->size()) {std::cerr<<"ERROR: bad plsize "<<plsize<<" > docs-size "<<docs->size()<<std::endl; exit(-1);}
      out<<plsize<<"\t"<<t<<std::endl;
    }
  }
};

static void usage() {std::cerr<<"Usage: ./msearch.exe [-k#] [-M] [-a#.#] [-dd] data.mindex < query.txt"<<std::endl<<"  where -k number to return, -M math, -a alpha math/text balance, -dd dump dictionary"<<std::endl; exit(-1);}

int main(int argc, char *argv[]) {
  if (argc<2) usage();
  MSearch ms; int s=1; bool dd=false;
  for (;;) {
    if (s<argc && strstr(argv[s],"-k")==argv[s]) { ms.setk(std::stof(argv[s]+2)); s++; }
    else if (s<argc && strstr(argv[s],"-M")==argv[s] && *(argv[s]+2)==0) { ms.bMath=true; s++; }
    else if (s<argc && strstr(argv[s],"-a")==argv[s]) { ms.setAlpha(std::stof(argv[s]+2)); s++; }
    else if (s<argc && strstr(argv[s],"-dd")==argv[s]) { dd=true; s++; }
    else if (argc-s!=1) usage();
    else break;
  }
  ms.input(argv[s]); // from mindex
  if (dd) { ms.dumpDictionary(); return 0; }
  // query from stdin (until end or empty line)
  std::cerr<<"Enter queries:"<<std::endl;
  for (;;) { std::string line; getline(std::cin, line); if (!std::cin||line.compare("")==0) break; ms.query(line); }
  return 0;
}
