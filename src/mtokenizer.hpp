// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <cstring> //for Windows
#include <vector>
#include <algorithm>

// == tokenizer ======================================================
// cut strings into tokens delimited by whitespace (no processing)

typedef uint8_t byte; typedef const byte cbyte; typedef const char cchar;

static int toweight(cchar* s, uint sl) { return std::stoi(std::string(s,sl)); }

// copy strings into internal storage
class StringList { protected: char* d; int dsize; int dused; public:
  inline StringList() { d=(char*)malloc(dsize=1<<20); dused=0; }
  virtual ~StringList() { if (d!=NULL) delete d; d=NULL; }
  inline char* addcopy(cchar* s, int sl) { // return pointer to internal copy
    if (dsize<dused+sl+1) {d=(char*)realloc(d,dsize*=2);} //grow
    char* scopy=d+dused; memcpy(scopy,s,sl); scopy[sl]='\0'; dused+=sl+1; return scopy; }
  inline char* addcopy(cchar* s) { return addcopy(s,strlen(s)); }
  inline void clear() { dused=0; }
};

class MTokenizer { protected:
  bool ws[256]; inline void set(bool* t, int s, int e, bool v=true) { for (int i=s;i<=e;i++) t[i]=v; }
  void setupArrays() { set(ws,0,255,false); ws[' ']=ws['\t']=ws['\r']=ws['\n']=true; }

public:
  class TokenList : protected StringList { protected:
    struct TKE { TKE(int S,int W) :s(S),w(W) {} int s, w; }; // token start relative to base, weight for that value
    std::vector<TKE> v; //relative to base so realloc works
    struct charcmp { cchar* d; charcmp(char* base) {d=base;}
      bool operator()(const TKE& a, const TKE& b) const { return strcmp(d+a.s, d+b.s)<0; } };
  public:
    inline void clear() { v.clear(); StringList::clear(); }
    inline void push_back(cbyte* s, int sl, int w) { v.push_back(TKE(addcopy((cchar*)s,sl)-d,w)); }
    inline size_t size() const { return v.size(); }
    inline int weight(int i) { return v[i].w; }
    inline cchar* const operator[](int i) const { return d+v[i].s; } // cannot edit values
    inline void sort() { std::sort(v.begin(), v.end(), charcmp(d)); }
    inline void dump() { for (int i=0;i<size();i++) { std::cerr<<(*this)[i]<<" "; } std::cerr<<std::endl; }
  };
  inline MTokenizer() { setupArrays(); }
  // used by minvert and msearch
  inline void process(cchar* data, int size,/*out*/TokenList& tokens) {
    cbyte* d=(cbyte*)data; cbyte* dend=d+size; int weight=1;
    for(;d<dend;d++) {
      if (!ws[*d]) { cbyte* s=d++;
        for(;;d++) { if (d>=dend || ws[*d]) { if (d-s>4&&s[0]=='#'&&s[1]=='!'&&d[-2]=='!'&&d[-1]=='#') weight=toweight((cchar*)s+2,d-(s+4)); else tokens.push_back(s,d-s,weight); break; } }
      }
    }
    //for (int i=0;i<tokens.size();i++) { cout<<tokens.at(i)<<" "; } cout<<endl;
  }
};
