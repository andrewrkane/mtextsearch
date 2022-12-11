// (C) Copyright 2019 Andrew R. J. Kane <arkane@uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <string>
#include <vector>
#include <set>

#include "porterstemmer.hpp"
using namespace std;

// == tokenizer ======================================================
// cut strings into tokens and do case-folding, stemming, expansion, etc.
// used on the indexing size and the querying side (and they have to agree)

typedef uint8_t byte; typedef const byte cbyte; typedef const char cchar;

inline char* tolower(/*modified*/char* s) { for (char* t=s;*t!='\0';t++) { *t=::tolower(*t); } return s; }
inline char* pstem(/*modified*/char* s, int sl) { int k=::stem(s,0,sl-1); s[k+1]='\0'; return s; } // porter stemmer

inline void loadkeywords(cchar* keywordsfile, /*in/out*/ set<string>& keywords) {
  ifstream in(keywordsfile); if (!in) {cerr<<"ERROR: missing T file "<<keywordsfile<<endl;exit(-1);}
  char b[1<<10];
  for (string line; getline(in,line);) {
    if (line.length()>=(1<<10)) { cerr<<"dropped keyword "<<line<<endl; continue; }
    string s=(string)pstem(tolower(strcpy(b,line.c_str())),line.length()); //TODO: remove whitespace? punctuation?
    keywords.insert(s);
  }
  cerr<<"loaded "<<keywords.size()<<" keywords from "<<keywordsfile<<endl;
}

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
  bool me[256], tk[256], tkmath[256]; inline void set(bool* t, int s, int e, bool v=true) { for (int i=s;i<=e;i++) t[i]=v; }
  void setupArrays() { set(me,0,255,false); me[' ']=me['\t']=me['\r']=me['\n']=me['#']=true;
    set(tk,0,127,false); set(tk,128,255); set(tk,'a','z'); set(tk,'A','Z'); set(tk,'0','9');
    for (int i=0;i<256;i++) tkmath[i]=tk[i]; tkmath['#']=true; } //tk['<']=true;

public:
  class TokenList : protected StringList { protected: vector<int> v; //relative to base so realloc works
    struct charcmp { cchar* d; charcmp(char* base) {d=base;} bool operator()(const int a, const int b) const { return strcmp(d+a, d+b)<0; } };
  public:
    inline void clear() { v.clear(); StringList::clear(); }
    inline void push_back(cbyte* s, int sl) { v.push_back(pstem(tolower(addcopy((cchar*)s,sl)),sl)-d); } // pstem + casefold
    inline void swap_remove(int i) { int last=v.size()-1; if (i<last) v[i]=v[last]; v.pop_back(); }
    inline void removenotin(const std::set<string>& keywords) {
      int drop=0; for (int i=0; i<size(); i++) { if ((*this)[i][0]!='#' && keywords.find((*this)[i])==keywords.end()) drop++; else v[i-drop]=v[i]; } v.resize(size()-drop);
    }
    inline void removenotin_dump(const std::set<string>& keywords) { cerr<<"removing: ";
      int drop=0; for (int i=0; i<size(); i++) { if ((*this)[i][0]!='#' && keywords.find((*this)[i])==keywords.end()) { cerr<<(*this)[i]<<" "; drop++; } else v[i-drop]=v[i]; } v.resize(size()-drop); cerr<<endl;
    }
    inline size_t size() const { return v.size(); }
    inline cchar* const operator[](int i) const { return d+v.at(i); } // cannot edit values
    inline void sort() { std::sort(v.begin(), v.end(), charcmp(d)); }
    inline void dump() { for (int i=0;i<size();i++) { cerr<<(*this)[i]<<" "; } cerr<<endl; }
  };
  inline MTokenizer() { setupArrays(); }
  // used by minvert
  inline void process(cchar* data, int size, bool bMath,/*out*/TokenList& tokens) {
    cbyte* d=(cbyte*)data; cbyte* dend=d+size;
    if (bMath) {
      for(;d<dend;d++) {
        if (tkmath[*d]) { cbyte* s=d++;
          if (*s=='#') { // math tuples
            for (;;d++) { if (d>=dend || me[*d]) { if (*d=='#') { tokens.push_back(s,d-s+1); } else { d=s; } break; } }
          } else { // non-math doesn't start with #
            for(;;d++) { if (d>=dend || !tkmath[*d]) { tokens.push_back(s,d-s); break; } }
          }
        }
      }
    } else {
      for(;d<dend;d++) {
        if (tk[*d]) { cbyte* s=d++;
          //if (d<dend && *(d-1)=='<' && *d=='/') d++; // end tags
          for(;;d++) { if (d>=dend || !tk[*d]) { tokens.push_back(s,d-s); break; } }
        }
      }
    }
    //for (int i=0;i<tokens.size();i++) { cout<<tokens.at(i)<<" "; } cout<<endl;
  }
};
