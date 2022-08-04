// (C) Copyright 2019 Andrew R. J. Kane <arkane@uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

// Templates not working for this so use preprocessor (DictionaryTwoLayer)

class DTL : public BaseTwoLayer { protected:
  struct KVEncoder : KEncoder { V lastv;
    inline KVEncoder(Data& data) :KEncoder(data) {}
    inline void newGroup() { KEncoder::newGroup(); lastv=V(); }
    inline void encode(cchar* c, cchar* lastc, V v) { KEncoder::encode(c,lastc); v.write(d,lastv); lastv=v; }
  };
  struct SkipEncoder : KVEncoder { byte* dstart; Skips& skips; int cgroup;
    inline SkipEncoder(Skips& skips) : KVEncoder(skips.data),skips(skips) { dstart=skips.data.d; cgroup=0; }
    inline void encode(cchar* c, cchar* lastc, V v) { cgroup--; if (cgroup<=0) { skips.s[skips.l++]=(d-dstart); cgroup=skips.skipsize; newGroup(); } KVEncoder::encode(c,lastc,v); } //split #terms
    inline void encodeEnd() { skips.s[skips.l]=(d-dstart); KVEncoder::encodeEnd(); } //encode endpoint for group size
  };
  struct SkipDecoder {
    static inline V getV(Skips& skips, cchar* c) { //prefix-suffix-pvbyte+delta-vbyte
      uint g=skips.getgroup((cbyte*)c);
      byte* d=skips.getd(g); byte* endd=skips.getd(g+1);
      //cout<<"find "<<c<<" "<<d<<endl;
      if (d>=endd) return V::UNKNOWN; //past group
      cchar* tc=c; V v; v.setid(g*skips.skipsize-1); for (;;) {
        if (*d==(cbyte)*tc) { if (*d++==0) { v.read(d); if (*tc==0) return v; else break; } tc++; } //found
        if (*d<(cbyte)*tc) { for(;*d++!=0;){}; v.read(d); break; }
        if (*d>(cbyte)*tc) return V::UNKNOWN; } //past
      //cout<<"-find match="<<match<<" v="<<v<<endl;
      //remaining
      for (;;){ if(d>=endd) return V::UNKNOWN; //past group
        uint pre,suff; readPVByte(d,pre,suff); byte* dsuffend=d+suff;
        //cout <<"--pre="<<pre<<" suff="<<suff<<" match="<<match<<" "<<c[match]<<" "<<*d<<endl;
        if (pre<(tc-c)) return V::UNKNOWN; //past
        else if (pre>(tc-c)) { d=dsuffend; v.read(d); continue; }
        else { for (;;) {
            if (*d==(cbyte)*tc) { d++; tc++; if (d>=dsuffend) { v.read(d); if (*tc==0) return v; else break; } } //found
            if (*d<(cbyte)*tc) { d=dsuffend; v.read(d); break; }
            if (*d>(cbyte)*tc) return V::UNKNOWN; } } } //past
    }
    static inline V getV(Skips& skips, int id) {
      byte* d=skips.getd(id/skips.skipsize); id%=skips.skipsize; V v;
      for (;*d!=0;d++) {} d++; v.read(d);
      if (id==0) return v; else id--;
      //remaining
      for (;;) { uint pre,suff; readPVByte(d,pre,suff); d+=suff; v.read(d);
        if (id==0) return v; else id--; }
    }
    static inline int getK(Skips& skips, int id, char* c, uint maxc) { //return length of string in c
      byte* d=skips.getd(id/skips.skipsize); id%=skips.skipsize; int i; V v;
      for (i=0;i<maxc;i++) { c[i]=(cchar)*d++; if (c[i]==0) break; } v.read(d);
      if (id==0) return i; else id--;
      //remaining
      for (;;) {
        uint pre,suff; readPVByte(d,pre,suff); byte* dsuffend=d+suff;
        for (i=pre;i<min(pre+suff,maxc);i++) { c[i]=(cchar)*d++; } c[i]=0; v.read(d);
        if (id==0) return i; else id--; }
    }
  };
  inline void read(ifstream& in, cchar* fn) { BaseTwoLayer::read(in,fn,DTLNAME); }
  SkipEncoder* enc;
public:
  inline void write(ofstream& out) { if (enc!=NULL) addEnd(); BaseTwoLayer::write(out,DTLNAME); }
  DTL(ifstream& in, cchar* fn) { read(in,fn); enc=NULL; } //from write(), cannot add new values
  DTL() { data.d=new byte[1<<30]; skips.s=new uint[1<<30]; skips.l=dictsize=0; skips.skipsize=16; enc=new SkipEncoder(skips); } //call add(), then addEnd() or write() when done //TODO: make growable+realloc?
  inline void add(cchar* c, cchar* lastc, V v) { enc->encode(c,lastc,v); dictsize++; } //in-order (string&value) except with getV(id)
  inline void addEnd() { enc->encodeEnd(); delete enc; enc=NULL; };
  inline V getV(cchar* c) { return SkipDecoder::getV(skips,c); }
  inline V getV(int id) { return SkipDecoder::getV(skips,id); }
  inline int getK(int id, char* c, int cmax) { return SkipDecoder::getK(skips,id,c,cmax); }
};
