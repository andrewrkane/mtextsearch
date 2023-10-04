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
      //if (m.endbyte>=m.allocsize) { std::cerr<<"ERROR"<<std::endl; }
    } else {
      MultiH& mo=*(MultiH*)data;
      if (mo.allocsize<=mo.endbyte+10) { mo.allocsize*=2; data=(byte*)realloc(data,mo.allocsize); }
      MultiH& m=*(MultiH*)data;
      byte* d=data+m.endbyte; writeVByte(d,id-m.lastid); writeVByte(d,freq);
      m.endbyte=d-data; m.size++; m.lastid=id;
      //if (m.endbyte>=m.allocsize) { std::cerr<<"ERROR"<<std::endl; }
    }
  }
  inline uint getsize() { return (*(uint*)data==1 ? 1 : ((MultiH*)data)->size); }
  inline void output(std::ostream& out) { byte oh[20]; // bytelength \n vbytes: (size,[lastid]) (freq,id)+
    if (*(uint*)data==1) {
      SingleH& h=*(SingleH*)data;
      byte* o=oh; writeVByte(o,1); writeVByte(o,h.id); writeVByte(o,h.freq); // (size,lastid,id,freq)
      uint blen=o-oh; out<<blen<<std::endl; out.write((cchar*)oh,o-oh);
    } else {
      MultiH& m=*(MultiH*)data;
      byte* o=oh; writeVByte(o,m.size); writeVByte(o,m.lastid); // (size,lastid)
      byte* d=data+sizeof(MultiH); int dsize=m.endbyte-sizeof(MultiH); // (delta-id,freq)+
      uint blen=o-oh+dsize; out<<blen<<std::endl; out.write((cchar*)oh,o-oh); out.write((cchar*)d,dsize);
    }
  }
  inline void dump() {
    if (*(uint*)data==1) { SingleH& h=*(SingleH*)data; std::cout<<"PL size="<<h.size<<" "<<h.id<<":"<<h.freq<<std::endl; }
    else { MultiH& m=*(MultiH*)data; byte* d=data+sizeof(MultiH);
      std::cout<<"PL size="<<m.size; for (int i=0;i<m.size;i++) { uint id=readVByte(d); uint freq=readVByte(d); std::cout<<" "<<id<<":"<<freq; } std::cout<<std::endl; } }
};

struct charcmp { bool operator()(cchar* a, cchar* b) const { return strcmp(a, b)<0; } };

class Dictionary : protected std::map<cchar*, PostingsList, charcmp> { protected:
  char* d; int dsize; int dused; std::vector<char*> dmore; //store copy of char* values here
 public:
  inline Dictionary() { d=new char[dsize=1<<20]; dused=0; }
  virtual ~Dictionary() { if (d!=NULL) delete d; d=NULL; for (int i=0;i<dmore.size();++i) delete dmore[i]; dmore.clear(); }
  inline int size() { return map::size(); }
  inline void add(cchar* token, int docid, int count) {
    std::map<cchar*,PostingsList>::iterator it = find(token);
    if (it==end()) { // copy token string if not exist in map
      int s=strlen(token)+1; if (dsize<dused+s) { dmore.push_back(d); d=new char[dsize]; dused=0; } //more space
      char* tokencopy=d+dused; strcpy(tokencopy,token); dused+=s;
      (*this)[tokencopy].add(docid,count);
    } else { it->second.add(docid,count); }
  }
  void output(std::ostream& out) { // uncompressed
    std::cerr<<"Outputting "<<size()<<" postings lists."<<std::endl;
    for (iterator it=begin();it!=end();++it) { out<<it->first<<"\t"; it->second.output(out); out<<std::endl; } }
};

class MInvert { public: bool bMath; protected: bool bkeywords; std::set<std::string> keywords;
  std::vector<std::string> docnames; std::vector<int> docsizes; uint64_t totalpostings; int empty;
  MTokenizer tokenizer; Dictionary dict;

  void doIndex(int docid, /*in*/MTokenizer::TokenList& tokens) {
    //int m=0; for (int i=0;i<tokens.size();++i) { if (strlen(tokens[i])>m) m=strlen(tokens[i]); } std::cout<<"max="<<m<<endl;
    //for (int i=0;i<tokens.size();++i) { std::cout<<tokens[i]<<" "; } std::cout<<std::endl;
    // determine frequency of tokens and add to postings lists
    tokens.sort();
    for (int i=0;i<tokens.size();) {
      cchar* token=tokens[i]; int count=1; i++;
      while (i<tokens.size() && strcmp(token,tokens[i])==0) { ++count; ++i; }
      //std::cerr<<token<<":"<<count<<std::endl;
      dict.add(token, docid, count);
    }
    //for (Dictionary::iterator it=dict.begin();it!=dict.end();++it) { std::cerr<<it->first<<":"<<it->second.size()<<std::endl; }
  }

  MTokenizer::TokenList tokens; // reuse between calls, might not be thread safe
  std::chrono::high_resolution_clock::time_point sp; //pacifier
  void doIndex(const std::string docname, /*in/edited*/ char* data, int size) {
    if (docname.compare("")==0) { std::cerr<<"ERROR: missing docname"<<std::endl; exit(-1); }
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
    if ((docid+1)%10000==0) { std::chrono::high_resolution_clock::time_point e=std::chrono::high_resolution_clock::now(); std::cerr<<(docid+1) <<" "<<std::chrono::duration_cast<std::chrono::milliseconds>(e-sp).count()<<"ms" <<" dictSize="<<dict.size() <<" totalpostings="<<totalpostings<<std::endl; sp=e; }
  }

  void doIndexTREC(std::istream& in, cchar* fn) {
    bool bwarning=false;
    sp=std::chrono::high_resolution_clock::now();
    uint64_t base=0; int s,e,dsize=1<<20; char* d=(char*)malloc(dsize+1); d[dsize]='\0'; //null-at-end
    in.read(d,4); s=0; e=in.gcount(); if (e<=0) {std::cerr<<"ERROR: Empty "<<fn<<std::endl; exit(-1);}
    if (e<4||memcmp(d,"<DOC>",4)!=0) {std::cerr<<"ERROR: format "<<fn<<" found "<<d<<std::endl; exit(-1);}
  READMORE:
    //std::cerr<<"READMORE"<<" s="<<s<<" e="<<e<<std::endl; //cerr.write(d,10); cerr<<std::endl;
    if (s>0) { base+=s; if (e>s) { memcpy(d,d+s,e-s); } e-=s; s=0; } //copy to front
    //std::cerr<<"- READMORE"<<" s="<<s<<" e="<<e<<std::endl;
    if (e==dsize) { dsize*=2; d=(char*)realloc(d,dsize+1); d[dsize]='\0'; if (dsize>1<<25) {std::cerr<<"ERROR: alloc too big, dumping buffer to stdout"<<std::endl; std::cout<<d; exit(-1);} } //grow //null-at-end
    in.read(d+e,dsize-e); int r=in.gcount(); e+=r;
    if (e>dsize) {std::cerr<<"ERROR: index read buffer overflow "<<e<<" "<<dsize<<std::endl; exit(-1);}
    d[e]='\0'; // null terminate
    for (char* n=d;n<d+e;++n) { if (*n=='\0') *n=' '; } //drop 0-bytes
    if (e==0) { if (d!=NULL) free(d); d=NULL; return; } //cleanup and exit
    if (r==0) { std::cerr<<"WARNING: invalid final DOC "<<(e-s)<<" "<<std::string(d+s,e-s)<<std::endl; return;}
    //process
    for (;;) {
      //find DOC & DOCNO & DOCHDR
      for (;s<e && isspace(d[s]); s++) {} //skip whitespace
      char* sdoc=strstr(d+s,"<DOC>"); if (sdoc==NULL) goto READMORE;
      if (s!=sdoc-d) {std::cerr<<"WARNING: non-whitespace between DOCs "<<(sdoc-d)-s<<" "<<std::string(d+s,(sdoc-d)-s)<<std::endl; bwarning=true;}
      char* edoc=strstr(sdoc+5,"</DOC>"); if (edoc==NULL) goto READMORE;
      char* sdocno=strstr(sdoc+5,"<DOCNO>"); if (sdocno==NULL||sdocno>=edoc) {std::cerr<<"ERROR: DOCNO missing"<<std::endl; exit(-1);}
      char* edocno=strstr(sdocno+7,"</DOCNO>"); if (edocno==NULL||sdocno>=edoc) {std::cerr<<"ERROR: /DOCNO missing"<<std::endl; exit(-1);}
      char* sdochdr=strstr(edocno+8,"<DOCHDR>");
      char* edochdr=strstr(edocno+8,"</DOCHDR>");
      char* content=(edochdr==NULL?edocno+8:edochdr+9); // content = </DOCHDR>...</DOC> or </DOCNO>...</DOC>
      //char* content=sdochdr+9; // content = <DOCHDR>...</DOC>
      char* docname=sdocno+7;
      //split data
      if (bwarning) { std::cerr<<"next doc "<<std::string(docname,edocno-docname)<<std::endl; bwarning=false; }
      //std::cerr<<"docname="<<std::string(docname,edocno-docname)<<" contentsize="<<edoc-content<<" content(300)="<<std::string(content,300)<<std::endl;
      doIndex(std::string(docname,edocno-docname),content,edoc-content);
      s=edoc-d+6;
    }
  }

public:
  MInvert() { bMath=false; bkeywords=false; totalpostings=0L; empty=0; }
  void setT(cchar* keywordsfile) { bkeywords=true; loadwords(keywordsfile, keywords); }

  void input(std::istream& in, cchar* fn) { doIndexTREC(in,fn); }

  void output(std::ostream& out) {
    out<<(bMath?"math":"text")<<".mindex.1"<<std::endl;
    int s=docnames.size(); std::cerr<<"Output "<<s<<" docs"<<std::endl;
    out<<s<<std::endl; for (int i=0;i<s;i++) { out<<docsizes[i]<<"\t"<<docnames[i]<<std::endl; } out<<std::endl;
    dict.output(out);
  }
};

static void usage() {
  std::cerr<<"Usage: ./minvert.exe [-T keywords.txt] [-M] datafile ... > out.mindex"<<std::endl;
  std::cerr<<"       ./minvert.exe [-T keywords.txt] [-M] < datafile > out.mindex"<<std::endl; exit(-1); }

int main(int argc, char *argv[]) {
  MInvert ms; int s=1;
  for (;;) {
    if (s<argc && strstr(argv[s],"-T")==argv[s]) { if (s+1>=argc) usage(); ms.setT(argv[s+1]); s+=2; }
    else if (s<argc && strstr(argv[s],"-M")==argv[s] && *(argv[s]+2)==0) { ms.bMath=true; s++; }
    else break;
  }
  if (argc-s==0) { ms.input(std::cin,"stdin"); } else if (argc-s>0) { for (;s<argc;s++) { std::ifstream in(argv[s]); if (!in) usage(); ms.input(in, argv[s]); } } else usage(); // input
  ms.output(std::cout); // output
  return 0;
}
