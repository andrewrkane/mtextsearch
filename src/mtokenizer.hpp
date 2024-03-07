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
  class TokenList : protected StringList { protected: std::vector<int> v; //relative to base so realloc works
    struct charcmp { cchar* d; charcmp(char* base) {d=base;} bool operator()(const int a, const int b) const { return strcmp(d+a, d+b)<0; } };
  public:
    inline void clear() { v.clear(); StringList::clear(); }
    inline void push_back(cbyte* s, int sl) { v.push_back(addcopy((cchar*)s,sl)-d); }
    inline size_t size() const { return v.size(); }
    inline cchar* const operator[](int i) const { return d+v.at(i); } // cannot edit values
    inline void sort() { std::sort(v.begin(), v.end(), charcmp(d)); }
    inline void dump() { for (int i=0;i<size();i++) { std::cerr<<(*this)[i]<<" "; } std::cerr<<std::endl; }
  };
  inline MTokenizer() { setupArrays(); }
  // used by minvert
  inline void process(cchar* data, int size,/*out*/TokenList& tokens) {
    cbyte* d=(cbyte*)data; cbyte* dend=d+size;
    for(;d<dend;d++) {
      if (!ws[*d]) { cbyte* s=d++;
        for(;;d++) { if (d>=dend || ws[*d]) { tokens.push_back(s,d-s); break; } }
      }
    }
    //for (int i=0;i<tokens.size();i++) { cout<<tokens.at(i)<<" "; } cout<<endl;
  }
};
