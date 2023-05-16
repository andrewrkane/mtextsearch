// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath> // for log()
#include <sys/stat.h>

#include "mtokenizer.hpp"
#include "mdictionary.hpp"
using namespace std;

/* read in mindex file, run queries from stdin, output DOCNO results to stdout */

struct Scored { int docid; float score; Scored(int d,float s) {docid=d; score=s;} }; Scored EmptyScore(-1,-1);
bool mincomp(const Scored& a, const Scored& b) { return a.score>b.score; } // score order
class TopkHeap : public vector<Scored> { public:
  TopkHeap(int topk=5) : vector<Scored>(topk,EmptyScore) {}
  inline void add(int docid, float score) { if (score>front().score) { pop_heap(begin(),end(),mincomp); pop_back(); push_back(Scored(docid,score)); push_heap(begin(),end(),mincomp); } }
  void done() { sort(begin(),end(),mincomp); }
  //void dump() { for (int i=0;i<size();i++) {cerr<<(*this)[i].docid<<":"<<(*this)[i].score<<" ";} cerr<<endl; }
};

struct Posting { int32_t id; int32_t freq; Posting(int id, int freq) { this->id=id; this->freq=freq; } };
class PLIter { byte* data; byte* d; byte* dend; Posting p; public: int plsize; float w;
  PLIter(DocnamesTwoLayer* docs, byte* data, int blen, float weight) : p(0,0), w(weight) { this->data=data; d=data; dend=d+blen; plsize=readVByte(d); int lastid=(plsize>1?readVByte(d):-1); next();
  }
  virtual ~PLIter() { delete data; data=d=dend=NULL; }
  inline const Posting& current() { return p; }
  inline bool next() { if (d>=dend) return false; p.id+=readVByte(d); p.freq=readVByte(d); return true; }
};
bool PLIComp(PLIter*& i, PLIter*& j) { return i->current().id < j->current().id; }
struct PLIV : public vector<PLIter*> { virtual ~PLIV() { for (int i=0;i<size();++i) {delete (*this)[i];} resize(0); } };

class MSearch { public: bool bMath; float alpha; protected: bool bkeywords; set<string> keywords; set<string> stopwords; int k;
  DocnamesTwoLayer* docs; uint64_t totaltokens; //docs(docname->docsize)
  ifstream* postfile; DictionaryTwoLayer* dict; //dict(token->location) points into postfile
  MTokenizer tokenizer;

  inline void getIterators(/*in*/MTokenizer::TokenList& tokens, /*out*/PLIV& listIters) {
    tokens.sort();
    for (int i=0;i<tokens.size();) {
      cchar* token=tokens[i]; int count=1; i++;
      while (i<tokens.size() && strcmp(token,tokens[i])==0) { ++count; ++i; }
      uint64_t loc=dict->getV(token);
      if (loc==IntDeltaV::UNKNOWN) continue;
      ifstream& in=*postfile; in.clear(); in.seekg(loc);
      string t; in>>t; if (strcmp(token,t.c_str())!=0) {cerr<<"ERROR: pointing to wrong token "<<token<<" -> "<<t<<endl; exit(-1);}
      int blen; in>>blen;
      { string line; getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: index extra postings info "<<token<<endl; exit(-1);} }
      byte* data=new byte[blen];
      in.read((char*)data,blen);
      { string line; getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: index extra postings "<<token<<endl; exit(-1);} }
      float weight=count/(count+10.0f); if (bMath) { weight*=(token[0]=='#'?alpha:1.0-alpha); }
      PLIter* pli=new PLIter(docs,data,blen,weight); listIters.push_back(pli); //iterator owns data array
      //cerr<<"Found \'"<<token<<"\' count="<<count<<" plsize="<<pli->plsize<<" bytelength="<<blen<<endl;
    }
  }

  void doQuery(/*in*/const string& prefix, /*in*/PLIV& listIters, ostream& out, int doccount, float avgDocSize) {
    // intersect iterators w scoring
    int count=listIters.size(), base=0; // below base are already finished iterating
    TopkHeap h(k);
    // precompute IDF for BM25
    for (int i=0;i<count;i++) { PLIter& pli=*listIters[i];
      pli.w*=log(1.0+((double)doccount-pli.plsize+0.5)/(pli.plsize+0.5));
      //cerr<<"idf*weight="<<pli.w<<endl;
    }
    while (base<count) {
      sort(listIters.begin()+base, listIters.end(), PLIComp);
      //for (int i=base;i<count;i++) {cerr<<listIters[i].second[listIters[i].first].first<<" ";} cerr<<endl;
      // score iterators at docid
      int docid; float score=0;
      for (int i=base; i<count; i++) {
        PLIter& pli=*listIters[i]; const Posting& p=pli.current(); if (i==base) docid=p.id; else if (p.id!=docid) break;
        // BM25 see https://en.wikipedia.org/wiki/Okapi_BM25
        float tf=p.freq*(1.2+1.0) / (p.freq + 1.2*(1.0 - 0.75 + 0.75*docs->getV(docid)/avgDocSize));
        //cerr<<"p.freq="<<p.freq<<" doclength="<<docs->getV(docid)<<" avgDocSize="<<avgDocSize<<endl;
        //cerr<<"tf="<<tf<<" tf*w="<<tf*pli.w<<endl;
        score += tf*pli.w;
        // advance iterators at docid
        if (!pli.next()) { swap(listIters[base], listIters[i]); base++; }
      }
      h.add(docid,score);
    }
    h.done();
    // output
    for (int i=0;i<h.size()&&h[i].docid>=0;i++) { char c[1<<10]; docs->getK(h[i].docid,c,1<<10); out<<prefix<<c<<"\t"<<i+1<<"\t"<<h[i].score<<endl; }
  }

public:
  MSearch() { bMath=false; alpha=0.18; docs=NULL; totaltokens=0; postfile=NULL; dict=NULL; bkeywords=false; k=10; }
  virtual ~MSearch() { if (docs!=NULL) delete docs; docs=NULL;
    if (dict!=NULL) delete dict; dict=NULL;
    if (postfile!=NULL) postfile->close(); postfile=NULL; }
  void setT(cchar* keywordsfile) { bkeywords=true; loadwords(keywordsfile, keywords); }
  void setS(cchar* stopwordsfile) { loadwords(stopwordsfile, stopwords); }
  void setk(int t) { if (t<=0) {cerr<<"ERROR: invalid k="<<t<<endl;exit(-1);} k=t; }
  void setAlpha(float a) { if (a<0||a>1) {cerr<<"ERROR: invalid alpha "<<a<<endl; exit(-1);} alpha=a; }

  void query(string query) {
    chrono::high_resolution_clock::time_point s=chrono::high_resolution_clock::now();
    cerr<<"query: "<<query<<endl;
    // named vs normal
    string prefix=""; size_t cut=query.find(';');
    if (cut!=string::npos) { prefix=query.substr(0,cut)+"\t"; query=query.substr(cut+1); }
    // split into tokens
    MTokenizer::TokenList tokens; tokenizer.process(query.c_str(),query.length(),bMath,tokens);
    if (stopwords.size()>0) tokens.removein_dump(stopwords); // remove stopwords
    if (bkeywords) tokens.removenotin_dump(keywords); // remove non-math token if not in keywords
    if (tokens.size()<=0) {cerr<<"empty query"<<endl; return;}
    //cerr<<"found "<<tokens.size()<<" tokens"<<endl;
    // stats
    int doccount=docs->size(); float avgDocSize=(double)totaltokens/doccount;
    //cerr<<"avgDocSize="<<avgDocSize<<endl;
    // find postings lists and query
    PLIV listIters; getIterators(tokens, listIters);
    //cerr<<"found "<<listIters.size()<<" lists"<<endl;
    doQuery(prefix, listIters, cout, doccount, avgDocSize);
    chrono::high_resolution_clock::time_point e=chrono::high_resolution_clock::now();
    cerr<<"Query took "<<(double)chrono::duration_cast<chrono::microseconds>(e-s).count()/1000<<"ms"<<endl;
  }

  void input(const char* fn) {
    chrono::high_resolution_clock::time_point s=chrono::high_resolution_clock::now();
    // postfile
    if (postfile!=NULL) {cerr<<"ERROR: only supporting one index file"<<endl; exit(-1);}
    postfile=new ifstream(fn); string line; //cerr<<"Input "<<fn<<endl;
    ifstream& in=*postfile; if (!in.is_open()) {cerr<<"ERROR: Could not open input file "<<fn<<endl; exit(-1);}
    getline(in,line); if (!in) {cerr<<"ERROR: Empty input file "<<fn<<endl; exit(-1);}
    if (line.compare((bMath?"math.mindex.1":"text.mindex.1"))!=0) {cerr<<"ERROR: Unknown file format "<<fn<<" "<<line<<endl; exit(-1);}
    // meta
    string metafn=(string)fn+".meta"; ifstream metain(metafn); if (!metain) {cerr<<"ERROR: loading meta file "<<metafn<<endl; exit(-1);}
    struct stat sb; int er=stat(fn,&sb); uint64_t fsize=(uint64_t)sb.st_size; uint64_t t; metain>>t; if (er==-1 || t!=fsize) {cerr<<"ERROR: meta "<<metafn<<" wrong size match for "<<fn<<endl; exit(-1);}
    getline(metain,line); if (line.compare("")!=0) {cerr<<"ERROR: meta "<<metafn<<" extra size match info "<<line<<endl; exit(-1);}
    docs=new DocnamesTwoLayer(metain,metafn.c_str()); metain>>totaltokens; getline(metain,line); if (line.compare("")!=0) {cerr<<"ERROR: meta "<<metafn<<" extra totaltokens "<<line<<endl; exit(-1);}
    dict=new DictionaryTwoLayer(metain,metafn.c_str()); metain.close();
    chrono::high_resolution_clock::time_point e=chrono::high_resolution_clock::now();
    //cerr<<"Input "<<metafn<<" took "<<(double)chrono::duration_cast<chrono::microseconds>(e-s).count()/1000 <<"ms"<<" (docs="<<docs->size()<<",tt="<<totaltokens<<",terms="<<dict->size()<<")"<<endl;
  }
};

static void usage() {cerr<<"Usage: ./msearch.exe [-T keywords.txt] [-S stopwords.txt] [-k#] [-M] [-a#.#] data.mindex < query.txt"<<endl; exit(-1);}

int main(int argc, char *argv[]) {
  if (argc<2) usage();
  MSearch ms; int s=1;
  for (;;) {
    if (s<argc && strstr(argv[s],"-T")==argv[s]) { if (s+1>=argc) usage(); ms.setT(argv[s+1]); s+=2; }
    else if (s<argc && strstr(argv[s],"-S")==argv[s]) { if (s+1>=argc) usage(); ms.setS(argv[s+1]); s+=2; }
    else if (s<argc && strstr(argv[s],"-k")==argv[s]) { ms.setk(stof(argv[s]+2)); s++; }
    else if (s<argc && strstr(argv[s],"-M")==argv[s] && *(argv[s]+2)==0) { ms.bMath=true; s++; }
    else if (s<argc && strstr(argv[s],"-a")==argv[s]) { ms.setAlpha(stof(argv[s]+2)); s++; }
    else if (argc-s!=1) usage();
    else break;
  }
  ms.input(argv[s]); // from mindex
  // query from stdin (until end or empty line)
  cerr<<"Enter queries:"<<endl;
  for (;;) { string line; getline(cin, line); if (!cin||line.compare("")==0) break; ms.query(line); }
  return 0;
}
