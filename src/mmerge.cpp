// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

/* read in mindex files, merge results (inline for low memory usage), output mindex */

typedef uint8_t byte; typedef const char cchar; typedef uint32_t uint;

inline static void writeVByte(byte*& d, uint v) { byte t[5]; int i=0;
  for (;;i++) { t[i]=v&0x7F;v>>=7; if(v==0)break; } for (;i>0;i--) {*d++=t[i]|0x80;} *d++=t[i]; }
inline static void writeVByte(std::ostream& out, uint v) { byte t[5]; int i=0; for (;;i++) { t[i]=v&0x7F;v>>=7; if(v==0)break; } for (;i>0;i--) {out.put(t[i]|0x80);} out.put(t[i]); }
inline static uint vbytesize(uint v) { byte t[5]; int i=0; for (;;i++) { t[i]=v&0x7F;v>>=7; if(v==0)break; } return i+1; }
inline static uint readVByte(byte*& d) { for (uint v=0;;d++) { v|=*d&0x7F; if ((*d&0x80)==0) { d++; return v; } v<<=7; } }

class MIndex { public:
  class DataH { public: int base,psize,lastid,firstid; byte* d; byte* dend;
    DataH() { base=0; reset(); }
    inline void reset() { psize=lastid=firstid=0; d=dend=NULL; }
    inline void reset(byte* data, int dsize) { d=data; dend=d+dsize; psize=readVByte(d); lastid=readVByte(d); firstid=(psize<=1?lastid:readVByte(d)); }
  };

  const char* fn; std::ifstream in; int doccount, plcount; //index level
  std::string token; byte* data; int dalloc; DataH h; //postings list level
  bool bMath;
  
  MIndex(const char* f) : in(f) { fn=f; doccount=plcount=0; token=""; data=(byte*)malloc(dalloc=1<<20);
    if (!in.is_open()) {std::cerr<<"ERROR: Could not open input file "<<fn<<std::endl; exit(-1);}
    // decide what type of file
    std::string line; getline(in, line);
    if (!in || line.compare("")==0) {std::cerr<<"ERROR: Empty input file "<<fn<<std::endl; exit(-1);}
    if (line.compare("text.mindex.1")==0) { bMath=false; }
    else if (line.compare("math.mindex.1")==0) { bMath=true; }
    else {std::cerr<<"ERROR: Unknown file format in "<<fn<<", found ("<<line<<")."<<std::endl; exit(-1);}
  }

  void read_doccount() { in>>doccount; std::string line; getline(in, line); }

  void readwrite_docsizenames(std::ostream& out) {
    for (int i=0;i<doccount;i++) { std::string line; getline(in, line); out<<line<<std::endl; } //pass through
    std::string line; getline(in, line);
    if (line.compare("")!=0) { std::cerr<<"ERROR: Invalid index file "<<fn<<", extra document names ("<<line<<")."<<doccount<<std::endl; }
    //drop newline except for last in index
  }

  bool read_tokendata() { // tokenname \t bytelength \n bytes
    in>>token; if (!in||token.compare("")==0) {std::cerr<<"Input "<<plcount<<" postings lists from "<<fn<<std::endl; return false;}
    uint dsize; in>>dsize;
    { std::string line; getline(in, line); if (line.compare("")!=0) {std::cerr<<"ERROR: Invalid index file "<<fn<<", extra postings info for "<<token<<"."<<std::endl; exit(-1);} }
    //read vbyte data
    while (dsize>dalloc) { data=(byte*)realloc(data,dalloc*=2); } //grow
    in.read((char*)data,dsize);
    h.reset(data,dsize);
    return true;
  }

  void read_endofpostings(std::ostream& out) {
    { std::string line; getline(in, line); if (line.compare("")!=0) {std::cerr<<"ERROR: Invalid index file "<<fn<<", extra postings for "<<token<<"."<<std::endl; exit(-1);} }
    plcount++;
  }
};

class AccumH { public: std::vector<MIndex::DataH*> dh; std::vector<uint> deltaid; uint psize,lastid;
  AccumH() { reset(); psize=-1; }
  inline void reset() { dh.clear(); }
  inline void add(MIndex::DataH& h) { dh.push_back(&h); }
  inline uint encodesetup() {
    deltaid.clear(); psize=lastid=0; uint blen=0;
    for (int i=0;i<dh.size();i++) { MIndex::DataH& h=*dh[i];
      psize+=h.psize;
      uint id=h.firstid+h.base-lastid; deltaid.push_back(id);
      blen+=vbytesize(id)+(h.dend-h.d);
      lastid=h.lastid+h.base;
    }
    return vbytesize(psize) + (psize>1?vbytesize(lastid):0) + blen;
  }
  inline void encode(std::ostream& out) {
    if (psize==-1) {std::cerr<<"ERROR: encode called before encodesetup."<<std::endl; exit(-1);}
    writeVByte(out,psize); if (psize>1) writeVByte(out,lastid);
    for (int i=0;i<dh.size();i++) { MIndex::DataH& h=*dh[i]; writeVByte(out,deltaid[i]); out.write((char*)h.d,h.dend-h.d); }
    reset(); lastid=-1;
  }
};

void output(std::ostream& out, std::vector<MIndex*>& ui) { // uncompressed
  int size=ui.size();
  // format
  for (int k=1;k<size;k++) { if (ui[k]->bMath!=ui[0]->bMath) {std::cerr<<"ERROR: Inconsistent file formats."<<std::endl; exit(-1);} }
  out<<(ui[0]->bMath?"math":"text")<<".mindex.1"<<std::endl;
  // doccount
  int base=0;
  for (int k=0;k<size;k++) { ui[k]->h.base=base; ui[k]->read_doccount(); base+=ui[k]->doccount; }
  std::cerr<<"Output "<<base<<" document names."<<std::endl;
  out<<base<<std::endl;

  // size+docnames
  for (int k=0;k<size;k++) { ui[k]->readwrite_docsizenames(out); } out<<std::endl; //end with newline

  // postings
  std::cerr<<"Output postings."<<std::endl;
  // setup first tokens
  for (int k=0;k<size;) {
    if (!ui[k]->in || !ui[k]->read_tokendata()) { /*bubbleup*/ for (int j=k+1;j<size;j++) { std::swap(ui[j],ui[j-1]); } size--; continue; } //done index file
    k++;
  }
  AccumH h;
  for (;size>0;) {
    // find 'lowest' token
    std::string token=ui[0]->token; h.reset(); h.add(ui[0]->h);
    for (int k=1;k<size;k++) {
      int sm = ui[k]->token.compare(token);
      if (sm < 0) { token=ui[k]->token; h.reset(); h.add(ui[k]->h); }
      else if (sm == 0) { h.add(ui[k]->h); }
    }
    // output 'lowest' token
    out<<token<<"\t"<<h.encodesetup()<<std::endl;
    h.encode(out);
    // advance all lists for 'lowest' token
    for (int k=0;k<size;) {
      if (ui[k]->token.compare(token)==0) {
        ui[k]->read_endofpostings(out);
        // get next tokens
        if (!ui[k]->in || !ui[k]->read_tokendata()) { /*bubbleup*/ for (int j=k+1;j<size;j++) { std::swap(ui[j],ui[j-1]); } size--; continue; } //done index file
      }
      k++;
    }
    out<<std::endl;
  }
}

static void usage() {
  std::cerr<<"Usage: ./mmerge.exe data.mindex ... > out.mindex"<<std::endl;
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc<=1) usage();
  std::string outflag="-o", outfile="";
  // setup input
  std::vector<MIndex*> ui;
  for (int i=1; i<argc; i++) { std::cerr<<"Input "<<argv[i]<<std::endl; ui.push_back(new MIndex(argv[i])); }
  // process and output inline
  output(std::cout, ui);
  std::cerr<<"Done output."<<std::endl;
  // cleanup
  for (int k=0;k<ui.size();k++) { delete ui[k]; }
  return 0;
}
