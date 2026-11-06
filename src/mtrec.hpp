// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>

/* read in TREC files, process each document via a callback interface */

typedef const char cchar;

class MTREC { public:
  virtual void processTREC(/*in/edited*/ char* sdata, char* edata, char* sdocno, char* edocno, char* sdochdr, char* edochdr, char* scontent, char* econtent)=0;
};

void doTREC(std::istream& in, cchar* fn, MTREC& cb) {
  bool bwarning=false;
  uint64_t base=0; int s,e,dsize=1<<20; char* d=(char*)malloc(dsize+1); d[dsize]='\0'; //null-at-end
  in.read(d,4); s=0; e=in.gcount(); if (e<=0) {std::cerr<<"ERROR: Empty "<<fn<<std::endl; exit(-1);}
  if (e<4||memcmp(d,"<DOC>",4)!=0) {std::cerr<<"ERROR: format "<<fn<<" found "<<d<<std::endl; exit(-1);}
READMORE:
  //std::cerr<<"READMORE"<<" s="<<s<<" e="<<e<<std::endl; //cerr.write(d,10); cerr<<std::endl;
  if (s>0) { base+=s; if (e>s) { memcpy(d,d+s,e-s); } e-=s; s=0; } //copy to front
  //std::cerr<<"- READMORE"<<" s="<<s<<" e="<<e<<std::endl;
  if (e==dsize) { dsize*=2; d=(char*)realloc(d,dsize+1); d[dsize]='\0'; if (dsize>1<<25) {std::cerr<<"ERROR: alloc too big, dumping buffer to stdout"<<std::endl; std::cout<<d; exit(-1);} } //grow //null-at-end
  in.read(d+e,std::min(1<<15,dsize-e)); int r=in.gcount(); e+=r; // 32k chunks
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
    cb.processTREC(sdoc,edoc+6, docname,edocno, (sdochdr==NULL?sdochdr:sdochdr+8),edochdr, content,edoc);
    s=edoc-d+6;
  }
}
