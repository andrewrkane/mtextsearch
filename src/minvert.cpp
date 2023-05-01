// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#include "mtokenizer.hpp"
using namespace std;

/* read in TREC files (optionally via mstrip), invert the text, output mindex file */

typedef uint8_t byte; typedef const char cchar; typedef uint32_t uint;

//byte* in dictionary: [size(==1),id,freq] or [bytealloc,endbyte,size(>1),lastid](delta-id,freq)+
// where square are uint (4-bytes) and curved are vbyte
class PostingsList { protected:
  byte* data;
  class SingleH { public: uint size,id,freq; };
  class MultiH { public: uint allocsize,endbyte,size,lastid; };
  inline static void writeVByte(byte*& d, uint v) { byte t[5]; int i=0;
    for (;;i++) { t[i]=v&0x7F;v>>=7; if(v==0)break; } for (;i>0;i--) {*d++=t[i]|0x80;} *d++=t[i]; }
  inline static uint readVByte(byte*& d) { for (uint v=0;;d++) { v|=*d&0x7F; if ((*d&0x80)==0) { d++; return v; } v<<=7; } }
public:
  inline PostingsList() { data=NULL; }
  inline void add(uint id, uint freq) {
    if (data==NULL) {
      data=(byte*)malloc(sizeof(SingleH)); SingleH& h=*(SingleH*)data; h.size=1; h.id=id; h.freq=freq;
    } else if (*(uint*)data==1) {
      data=(byte*)realloc(data,2*sizeof(MultiH)); SingleH& h=*(SingleH*)data;
      byte* d=data+sizeof(MultiH); writeVByte(d,h.id); writeVByte(d,h.freq); writeVByte(d,id-h.id); writeVByte(d,freq);
      MultiH& m=*(MultiH*)data; m.allocsize=2*sizeof(MultiH); m.endbyte=d-data; m.size=2; m.lastid=id;
      //if (m.endbyte>=m.allocsize) { cerr<<"ERROR"<<endl; }
    } else {
      MultiH& mo=*(MultiH*)data;
      if (mo.allocsize<=mo.endbyte+10) { mo.allocsize*=2; data=(byte*)realloc(data,mo.allocsize); }
      MultiH& m=*(MultiH*)data;
      byte* d=data+m.endbyte; writeVByte(d,id-m.lastid); writeVByte(d,freq);
      m.endbyte=d-data; m.size++; m.lastid=id;
      //if (m.endbyte>=m.allocsize) { cerr<<"ERROR"<<endl; }
    }
  }
  inline uint getsize() { return (*(uint*)data==1 ? 1 : ((MultiH*)data)->size); }
  inline void output(ostream& out) { byte oh[20]; // bytelength \n vbytes: (size,[lastid]) (freq,id)+
    if (*(uint*)data==1) {
      SingleH& h=*(SingleH*)data;
      byte* o=oh; writeVByte(o,1); writeVByte(o,h.id); writeVByte(o,h.freq); // (size,lastid,id,freq)
      uint blen=o-oh; out<<blen<<endl; out.write((cchar*)oh,o-oh);
    } else {
      MultiH& m=*(MultiH*)data;
      byte* o=oh; writeVByte(o,m.size); writeVByte(o,m.lastid); // (size,lastid)
      byte* d=data+sizeof(MultiH); int dsize=m.endbyte-sizeof(MultiH); // (delta-id,freq)+
      uint blen=o-oh+dsize; out<<blen<<endl; out.write((cchar*)oh,o-oh); out.write((cchar*)d,dsize);
    }
  }
  inline void dump() {
    if (*(uint*)data==1) { SingleH& h=*(SingleH*)data; cout<<"PL size="<<h.size<<" "<<h.id<<":"<<h.freq<<endl; }
    else { MultiH& m=*(MultiH*)data; byte* d=data+sizeof(MultiH);
      cout<<"PL size="<<m.size; for (int i=0;i<m.size;i++) { uint id=readVByte(d); uint freq=readVByte(d); cout<<" "<<id<<":"<<freq; } cout<<endl; } }
};

struct charcmp { bool operator()(cchar* a, cchar* b) const { return strcmp(a, b)<0; } };

class Dictionary : protected map<cchar*, PostingsList, charcmp> { protected:
  char* d; int dsize; int dused; vector<char*> dmore; //store copy of char* values here
 public:
  inline Dictionary() { d=new char[dsize=1<<20]; dused=0; }
  virtual ~Dictionary() { if (d!=NULL) delete d; d=NULL; for (int i=0;i<dmore.size();++i) delete dmore[i]; dmore.clear(); }
  inline int size() { return map::size(); }
  inline void add(cchar* token, int docid, int count) {
    map<cchar*,PostingsList>::iterator it = find(token);
    if (it==end()) { // copy token string if not exist in map
      int s=strlen(token)+1; if (dsize<dused+s) { dmore.push_back(d); d=new char[dsize]; dused=0; } //more space
      char* tokencopy=d+dused; strcpy(tokencopy,token); dused+=s;
      (*this)[tokencopy].add(docid,count);
    } else { it->second.add(docid,count); }
  }
  void output(ostream& out) { // uncompressed
    cerr<<"Outputting "<<size()<<" postings lists."<<endl;
    for (iterator it=begin();it!=end();++it) { out<<it->first<<"\t"; it->second.output(out); out<<endl; } }
};

class MInvert { public: bool bMath; protected: bool bkeywords; set<string> keywords;
  vector<string> docnames; vector<int> docsizes; uint64_t totalpostings; int empty;
  MTokenizer tokenizer; Dictionary dict;

  void doIndex(int docid, /*in*/MTokenizer::TokenList& tokens) {
    //int m=0; for (int i=0;i<tokens.size();++i) { if (strlen(tokens[i])>m) m=strlen(tokens[i]); } cout<<"max="<<m<<endl;
    //for (int i=0;i<tokens.size();++i) { cout<<tokens[i]<<" "; } cout<<endl;
    // determine frequency of tokens and add to postings lists
    tokens.sort();
    for (int i=0;i<tokens.size();) {
      cchar* token=tokens[i]; int count=1; i++;
      while (i<tokens.size() && strcmp(token,tokens[i])==0) { ++count; ++i; }
      //cerr<<token<<":"<<count<<endl;
      dict.add(token, docid, count);
    }
    //for (Dictionary::iterator it=dict.begin();it!=dict.end();++it) { cerr<<it->first<<":"<<it->second.size()<<endl; }
  }

  MTokenizer::TokenList tokens; // reuse between calls, might not be thread safe
  chrono::high_resolution_clock::time_point sp; //pacifier
  void doIndex(const string docname, /*in/edited*/ char* data, int size) {
    if (docname.compare("")==0) { cerr<<"ERROR: missing docname"<<endl; exit(-1); }
    // split into tokens
    tokens.clear(); tokenizer.process(data,size,bMath,tokens);
    if (bkeywords) tokens.removenotin(keywords); // remove non-math token if not in keywords
    if (tokens.size()<=0) { empty++; return; } // drop empty
    // setup document metadata
    int docid=docnames.size(); docnames.push_back(docname);
    docsizes.push_back(tokens.size()); totalpostings+=tokens.size();
    // add to index
    doIndex(docid, tokens);
    // pacifier
    if ((docid+1)%10000==0) { chrono::high_resolution_clock::time_point e=chrono::high_resolution_clock::now(); cerr<<(docid+1) <<" "<<chrono::duration_cast<chrono::milliseconds>(e-sp).count()<<"ms" <<" dictSize="<<dict.size() <<" totalpostings="<<totalpostings<<endl; sp=e; }
  }

  void doIndexTREC(istream& in, cchar* fn) {
    bool bwarning=false;
    sp=chrono::high_resolution_clock::now();
    uint64_t base=0; int s,e,dsize=1<<20; char* d=(char*)malloc(dsize+1); d[dsize]='\0'; //null-at-end
    in.read(d,4); s=0; e=in.gcount(); if (e<=0) {cerr<<"ERROR: Empty "<<fn<<endl; exit(-1);}
    if (e<4||memcmp(d,"<DOC>",4)!=0) {cerr<<"ERROR: format "<<fn<<" found "<<d<<endl; exit(-1);}
  READMORE:
    //cerr<<"READMORE"<<" s="<<s<<" e="<<e<<endl; //cerr.write(d,10); cerr<<endl;
    if (s>0) { base+=s; if (e>s) { memcpy(d,d+s,e-s); } e-=s; s=0; } //copy to front
    //cerr<<"- READMORE"<<" s="<<s<<" e="<<e<<endl;
    if (e==dsize) { dsize*=2; d=(char*)realloc(d,dsize+1); d[dsize]='\0'; if (dsize>1<<25) {cerr<<"ERROR: alloc too big, dumping buffer to stdout"<<endl; cout<<d; exit(-1);} } //grow //null-at-end
    in.read(d+e,dsize-e); int r=in.gcount(); e+=r;
    if (e>dsize) {cerr<<"ERROR: index read buffer overflow "<<e<<" "<<dsize<<endl; exit(-1);}
    d[e]='\0'; // null terminate
    for (char* n=d;n<d+e;++n) { if (*n=='\0') *n=' '; } //drop 0-bytes
    if (e==0) { if (d!=NULL) free(d); d=NULL; return; } //cleanup and exit
    if (r==0) { cerr<<"WARNING: invalid final DOC "<<(e-s)<<" "<<string(d+s,e-s)<<endl; return;}
    //process
    for (;;) {
      //find DOC & DOCNO & DOCHDR
      for (;s<e && isspace(d[s]); s++) {} //skip whitespace
      char* sdoc=strstr(d+s,"<DOC>"); if (sdoc==NULL) goto READMORE;
      if (s!=sdoc-d) {cerr<<"WARNING: non-whitespace between DOCs "<<(sdoc-d)-s<<" "<<string(d+s,(sdoc-d)-s)<<endl; bwarning=true;}
      char* edoc=strstr(sdoc+5,"</DOC>"); if (edoc==NULL) goto READMORE;
      char* sdocno=strstr(sdoc+5,"<DOCNO>"); if (sdocno==NULL||sdocno>=edoc) {cerr<<"ERROR: DOCNO missing"<<endl; exit(-1);}
      char* edocno=strstr(sdocno+7,"</DOCNO>"); if (edocno==NULL||sdocno>=edoc) {cerr<<"ERROR: /DOCNO missing"<<endl; exit(-1);}
      char* sdochdr=strstr(edocno+8,"<DOCHDR>");
      char* edochdr=strstr(edocno+8,"</DOCHDR>");
      char* content=(edochdr==NULL?edocno+8:edochdr+9); // content = </DOCHDR>...</DOC> or </DOCNO>...</DOC>
      //char* content=sdochdr+9; // content = <DOCHDR>...</DOC>
      char* docname=sdocno+7;
      //split data
      if (bwarning) { cerr<<"next doc "<<string(docname,edocno-docname)<<endl; bwarning=false; }
      //cerr<<"docname="<<string(docname,edocno-docname)<<" contentsize="<<edoc-content<<" content(300)="<<string(content,300)<<endl;
      doIndex(string(docname,edocno-docname),content,edoc-content);
      s=edoc-d+6;
    }
  }

public:
  MInvert() { bMath=false; bkeywords=false; totalpostings=0L; empty=0; }
  void setT(cchar* keywordsfile) { bkeywords=true; loadwords(keywordsfile, keywords); }

  void input(istream& in, cchar* fn) { doIndexTREC(in,fn); }

  void output(ostream& out) {
    out<<(bMath?"math":"text")<<".mindex.1"<<endl;
    int s=docnames.size(); cerr<<"Output "<<s<<" docs"<<endl;
    out<<s<<endl; for (int i=0;i<s;i++) { out<<docsizes[i]<<"\t"<<docnames[i]<<endl; } out<<endl;
    dict.output(out);
  }
};

static void usage() {
  cerr<<"Usage: ./minvert.exe [-T keywords.txt] [-M] datafile ... > out.mindex"<<endl;
  cerr<<"       ./minvert.exe [-T keywords.txt] [-M] < datafile > out.mindex"<<endl; exit(-1); }

int main(int argc, char *argv[]) {
  MInvert ms; int s=1;
  for (;;) {
    if (s<argc && strstr(argv[s],"-T")==argv[s]) { if (s+1>=argc) usage(); ms.setT(argv[s+1]); s+=2; }
    else if (s<argc && strstr(argv[s],"-M")==argv[s] && *(argv[s]+2)==0) { ms.bMath=true; s++; }
    else break;
  }
  if (argc-s==0) { ms.input(cin,"stdin"); } else if (argc-s>0) { for (;s<argc;s++) { ifstream in(argv[s]); if (!in) usage(); ms.input(in, argv[s]); } } else usage(); // input
  ms.output(cout); // output
  return 0;
}
