// (C) Copyright 2019 Andrew R. J. Kane <arkane@uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <string>
#include <vector>

#include "porterstemmer.hpp"
using namespace std;

// == tokenizer ======================================================
// cut strings into tokens and do case-folding, stemming, expansion, etc.
// used on the indexing size and the querying side (and they have to agree)

typedef uint8_t byte; typedef const byte cbyte; typedef const char cchar;

inline char* tolower(char* s) { for (char* t=s;*t!='\0';t++) { *t=::tolower(*t); } return s; }
inline char* pstem(char* s, int sl) { int k=::stem(s,0,sl-1); s[k+1]='\0'; return s; } // porter stemmer

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
  bool tk[256]; inline void setTK(int s, int e, bool v) { for (int i=s;i<=e;i++) tk[i]=v; }
  void setupTK() { setTK(0,127,false); setTK(128,255,true); // AK: allow any extended to support UTF-8
    setTK('a','z',true); setTK('A','Z',true); setTK('0','9',true); } //tk['<']=true;

public:
  class TokenList : protected StringList { protected: vector<int> v; //relative to base so realloc works
    struct charcmp { cchar* d; charcmp(char* base) {d=base;} bool operator()(int a, int b) const { return strcmp(d+a, d+b)<0; } };
  public:
    inline void clear() { v.clear(); StringList::clear(); }
    inline void push_back(cbyte* s, int sl) { v.push_back(pstem(tolower(addcopy((cchar*)s,sl)),sl)-d); } // pstem + casefold
    inline size_t size() const { return v.size(); }
    inline cchar* const operator[](int i) const { return d+v.at(i); } // cannot edit values
    inline void sort() { std::sort(v.begin(), v.end(), charcmp(d)); }
  };
  inline MTokenizer() { setupTK(); }
  // used by minvert
  inline void processI(cchar* data, int size, /*out*/TokenList& tokens) {
    cbyte* d=(cbyte*)data; cbyte* dend=d+size;
    for(;d<dend;d++) {
      if (tk[*d]) {
        cbyte* s=d++;
        for(;;d++) { if (d>=dend || !tk[*d]) { tokens.push_back(s,d-s); break; } }
      }
    }
    //for (int i=0;i<tokens.size();i++) { cout<<tokens.at(i)<<" "; } cout<<endl;
  }
  // used by msearch
  inline void processS(string data, /*out*/TokenList& tokens) { processI(data.c_str(),data.length(),tokens); }
};
