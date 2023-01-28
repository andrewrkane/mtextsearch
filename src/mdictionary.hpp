// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
using namespace std;

// == ListTwoLayer, DocnamesTwoLayer, DictionaryTwoLayer ======================================================
// array of pointers into single data block of groups, then search in array to get group, then values.
// - dictionary (string->value(int64delta)) approach described in textbook: Information Retrieval, Buttcher et al.
// - adapted to also implement list (index->string)

typedef uint8_t byte; typedef const byte cbyte; typedef const char cchar; typedef uint32_t uint;

inline static int strcmp(cbyte* a, cbyte* b) { return ::strcmp((char*)a,(char*)b); }
inline static void writeBytes(byte*& d, cbyte* b, uint l) { memcpy(d,b,l); d+=l; }
inline static void writeVByte(byte*& d, uint64_t v) { byte t[10]; int i=0; for (;;i++) { t[i]=v&0x7F;v>>=7; if(v==0)break; } for (;i>0;i--) {*d++=t[i]|0x80;} *d++=t[i]; }
inline static void writePVByte(byte*& d, uint a, uint b) {
  if (a<15) { if (b<15) { *d++=(a&0xF)<<4|(b&0xF); } else { *d++=(a&0xF)<<4|0xF; writeVByte(d,b-15); } }
  else { if (b<15) { *d++=0xF0|(b&0xF); writeVByte(d,a-15); } else { *d++=0xFF; writeVByte(d,a-15); writeVByte(d,b-15); } } }
inline static uint64_t readVByte(byte*& d) { for (uint64_t v=0;;d++) { v|=*d&0x7F; if ((*d&0x80)==0) { d++; return v; } v<<=7; } }
inline static void readPVByte(byte*& d, uint& a, uint& b) { a=(*d>>4)&0xF; b=*d++&0xF; if (a==15) a+=readVByte(d); if (b==15) b+=readVByte(d); }

class BaseTwoLayer { protected:
  class Data { public: byte* d; }; // cast to char* for strcmp
  class Skips { public: Data& data; uint* s; uint l; uint skipsize; inline Skips(Data& d):data(d){}
    inline byte* getd(int g) { return data.d+s[g]; }
    inline uint getgroup(cbyte* c, uint l, uint h) { for (;l<h-1;) { uint p=(l+h)/2; if (strcmp(getd(p),c)>0) h=p; else l=p; } return l; } //binary search in skips
    inline uint getgroup(cbyte* c) { return getgroup(c,0,l); } //full
  }; //index values into data array, l=lastskip of data block
  //stored-data
  Data data; Skips skips; uint dictsize;
  inline void write(ofstream& out, cchar* type) { out<<type<<endl;
    uint slen=skips.l+1, dlen=skips.s[skips.l]+1; out<<slen<<"\t"<<dlen<<"\t"<<skips.skipsize<<"\t"<<dictsize<<endl;
    out.write((cchar*)skips.s,slen*sizeof(uint)); out<<endl;
    out.write((cchar*)data.d,dlen); out<<endl; }
  inline void read(ifstream& in, cchar* fn, cchar* type) { string line;
    getline(in,line); if (!in) {cerr<<"ERROR: Empty input file "<<fn<<endl; exit(-1);}
    if (line.compare(type)!=0) {cerr<<"ERROR: Unknown format "<<fn<<" "<<type<<" "<<line<<endl; exit(-1);}
    uint slen,dlen; in>>slen; in>>dlen; in>>skips.skipsize; in>>dictsize; getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: dict "<<fn<<" info "<<line<<endl; exit(-1);}
    skips.l=slen-1; skips.s=new uint[slen]; in.read((char*)skips.s,slen*sizeof(uint));
    getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: dict "<<fn<<" skips "<<line<<endl; exit(-1);}
    data.d=new byte[dlen]; in.read((char*)data.d,dlen);
    getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: dict "<<fn<<" data "<<line<<endl; exit(-1);}
  }
  struct KEncoder { byte* d; bool first;
    inline KEncoder(Data& data) { d=data.d; newGroup(); }
    inline void newGroup() { first=true; }
    inline void encode(cchar* c, cchar* lastc) { cchar* tc=c; //prefix-suffix-pvbyte+delta-vbyte
      //cout<<"write "<<c<<" lastc="<<lastc<<" v="<<v<<" deltav="<<(v-lastv)<<endl;
      if (first) { for (;;tc++) { *d++=*tc; if(*tc==0)break; } first=false; }
      else { for (;*tc==*lastc;tc++,lastc++){} uint pre=tc-c; uint suff=strlen(tc);
        writePVByte(d,pre,suff); writeBytes(d,(cbyte*)tc,suff); //cout <<"-p="<<pre<<" s="<<suff<<" "<<tc<<endl;
      } }
    inline void encodeEnd() { *d++=0; } //null terminate
  };
public:
  inline BaseTwoLayer() : skips(data) {}
  virtual ~BaseTwoLayer() { delete data.d; data.d=NULL; delete skips.s; skips.s=NULL; }
  inline uint memoryusage() { return sizeof(BaseTwoLayer) +sizeof(uint)*(skips.l+1) +skips.s[skips.l]+1; }
  inline uint size() { return dictsize; }
};

// == ListTwoLayer ======================================================

struct EmptyV { int id; EmptyV(uint64_t v=0) {id=0;} inline void setid(int v) {id=v;}
  inline void write(byte*& d, EmptyV& lastv) {}
  inline void read(byte*& d) { id++; }
  static const int UNKNOWN = -1;
};
#define V EmptyV
#define DTL ListTwoLayer
#define DTLNAME "ListTwoLayer"
#include "dtl.hpp"
#undef DTLNAME
#undef DTL
#undef V

// == DocnamesTwoLayer ======================================================

struct IntV { uint64_t v; IntV(uint64_t v=0) {this->v=v;} inline void setid(int id) {}
  operator uint64_t() { return v; }
  inline void write(byte*& d, IntV& lastv) { writeVByte(d,v); }
  inline void read(byte*& d) { v=readVByte(d); }
  static const uint64_t UNKNOWN = (uint64_t)-1L;
};
#define V IntV
#define DTL DocnamesTwoLayer
#define DTLNAME "DocnamesTwoLayer"
#include "dtl.hpp"
#undef DTLNAME
#undef DTL
#undef V

// == DictionaryTwoLayer ======================================================

struct IntDeltaV { uint64_t v; IntDeltaV(uint64_t v=0) {this->v=v;} inline void setid(int id) {}
  operator uint64_t() { return v; }
  inline void write(byte*& d, IntDeltaV& lastv) { writeVByte(d,v-lastv.v); }
  inline void read(byte*& d) { v+=readVByte(d); }
  static const uint64_t UNKNOWN = (uint64_t)-1L;
};
#define V IntDeltaV
#define DTL DictionaryTwoLayer
#define DTLNAME "DictionaryTwoLayer"
#include "dtl.hpp"
#undef DTLNAME
#undef V
#undef DTL
