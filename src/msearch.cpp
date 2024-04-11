// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cmath> // for log()
#include <algorithm>
#include <chrono>
#include <unistd.h> // for close
#include <sys/stat.h>
#include <sys/fcntl.h> // for O_RDONLY
#include <sys/mman.h> // for mmap

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
class PLIV : public std::vector<PLIter> {};

class MSearch { public: bool bMath; float alpha; protected: int k;
  DocnamesTwoLayer* docs; uint64_t totaltokens; //docs(docname->docsize)
  int pffd; char* mmpf; int64_t pfsize; //memory map of postfile
  DictionaryTwoLayer* dict; //dict(token->location) points into postfile
  MTokenizer tokenizer;

  // TODO: assumes token sizes are less than 2^14
  inline std::string getlinepf(char*& x /*in/out*/) { char* e=x; for (;e<x+(1<<14);e++) { if (*e=='\n') break; } std::string r=std::string(x,e-x); if (*e=='\n') x=e; return r; }

  inline PLIter loadPL(uint64_t loc, float weight, /*out*/std::string& t) {
    if (loc>=pfsize) {std::cerr<<"ERROR: bad location "<<loc<<" max is "<<pfsize<<std::endl; exit(-1);}
    char* x=mmpf+loc; std::istringstream in(getlinepf(x));
    if (*x!='\n') {std::cerr<<"ERROR: bad token location data "<<std::string(x,1<<14)<<std::endl; exit(-1);}
    // TODO: make index handle tokens with spaces
    x++; in>>t; int blen; in>>blen;
    { std::string line; getline(in,line); if (line.compare("")!=0) {std::cerr<<"ERROR: index extra postings info "<<t<<" "<<line<<std::endl; exit(-1);} }
    volatile char touch=0; for (char* p=x; p<x+blen; p+=1<<12) { touch+=*p; } //force load into memory
    { if (*(x+blen)!='\n') {std::cerr<<"ERROR: index extra postings "<<t<<std::endl; exit(-1);} }
    PLIter pli((byte*)x,blen,weight);
    if (pli.plsize>docs->size()) {std::cerr<<"ERROR: plsize "<<pli.plsize<<" > docs.size "<<docs->size()<<std::endl; exit(-1);}
    return pli;
  }

  inline void getIterators(/*in*/MTokenizer::TokenList& tokens, /*out*/PLIV& listIters) {
    tokens.sort();
    for (int i=0;i<tokens.size();) {
      cchar* token=tokens[i]; int w=tokens.weight(i); i++;
      while (i<tokens.size() && strcmp(token,tokens[i])==0) { w+=tokens.weight(i); ++i; }
      float weight=w/(w+10.0f); if (bMath) { weight*=(token[0]=='#'?alpha:1.0f-alpha); }
      uint64_t loc=dict->getV(token);
      if (loc==IntDeltaV::UNKNOWN) continue; //not-in-data

      std::string t; PLIter pli=loadPL(loc,weight,t);
      if (strcmp(token,t.c_str())!=0) {std::cerr<<"ERROR: pointing to wrong token "<<token<<" -> "<<t<<std::endl; exit(-1);}
      listIters.push_back(pli);
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
        if ((score+Smax)<=T) { goto ADVANCE_SCORED; }
      }
      if (h.add(docid,score)) { T=h.front().score; }
      ADVANCE_SCORED:
      for (int i=0; i<=Pi; i++) { PLIter& pli=*X[i];
        if (!pli.next()) { X.erase(X.begin()+i); i--; Pi--; }
      }
    }
    h.done();
    // output
    for (int i=0;i<h.size()&&h[i].docid>=0;i++) { char c[1<<10]; docs->getK(h[i].docid,c,1<<10); out<<prefix<<c<<"\t"<<i+1<<"\t"<<h[i].score<<std::endl; }
  }

public:
  MSearch() { bMath=false; alpha=0.18f; docs=NULL; totaltokens=0; dict=NULL; pffd=-1; mmpf=NULL; pfsize=0; k=10; }
  virtual ~MSearch() { if (docs!=NULL) delete docs; docs=NULL;
    if (dict!=NULL) delete dict; dict=NULL;
    if (mmpf!=NULL) munmap(mmpf,pfsize); mmpf=NULL;
    if (pffd>=0) close(pffd); pffd=-1; pfsize=0; }
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
    std::string line;
    // postfile
    if (pffd>=0 || mmpf!=NULL || pfsize!=0) {std::cerr<<"ERROR: only supporting one index file"<<std::endl; exit(-1);}
    pffd=open(fn,O_RDONLY); struct stat sbindex; fstat(pffd, &sbindex); pfsize=sbindex.st_size;
    if (pffd<0) {std::cerr<<"ERROR: Could not open input file "<<fn<<std::endl; exit(-1);}
    mmpf=(char*)mmap(NULL, pfsize, PROT_READ, MAP_SHARED, pffd, 0);
    if (mmpf==MAP_FAILED) {std::cerr<<"ERROR: failed memory map of index file "<<fn<<std::endl; exit(-1);}
    std::cerr<<"Mapped index "<<fn<<" size "<<pfsize<<std::endl;
    char* x=mmpf; line=getlinepf(x); if (*x!='\n') {std::cerr<<"ERROR: Bad or empty input file "<<fn<<std::endl; exit(-1);}
    if (!(bMath && line.compare("math.mindex.1")==0) && line.compare("text.mindex.1")!=0) {std::cerr<<"ERROR: Unknown file format "<<fn<<" "<<line<<std::endl; exit(-1);} // external math tokenizer goes to text.mindex.1
    //volatile char touch=0; for (char* p=mmpf; p<mmpf+pfsize; p+=1<<12) { touch+=*p; } //force load into memory
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
      uint64_t loc=dict->getV(i);
      std::string t; PLIter pli=loadPL(loc,0.0f,t);
      out<<pli.plsize<<"\t"<<t<<std::endl;
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
