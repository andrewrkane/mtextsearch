// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>

#include "porterstemmer.hpp"

// == tokenize ======================================================
// cut strings into tokens and do case-folding, stemming, expansion, etc.
// used on the indexing side and the querying side (and they have to agree)

typedef uint8_t byte; typedef const byte cbyte; typedef const char cchar;

inline char* tolower(/*modified*/char* s) { for (char* t=s;*t!='\0';t++) { *t=::tolower(*t); } return s; }
inline char* pstem(/*modified*/char* s, int sl) { int k=::stem(s,0,sl-1); s[k+1]='\0'; return s; } // porter stemmer

class MTokenize { protected:
  bool me[256], tk[256], tkmath[256]; inline void set(bool* t, int s, int e, bool v=true) { for (int i=s;i<=e;i++) t[i]=v; }
  void setupArrays() { set(me,0,255,false); me[' ']=me['\t']=me['\r']=me['\n']=me['#']=true;
    set(tk,0,127,false); set(tk,128,255); set(tk,'a','z'); set(tk,'A','Z'); set(tk,'0','9');
    for (int i=0;i<256;i++) tkmath[i]=tk[i]; tkmath['#']=true; } //tk['<']=true;

public:
  inline MTokenize() { setupArrays(); }

  void process(std::string& line, bool bMath) {
    char* data = strdup(line.c_str()); int size=line.size(); // new owned array TODO: read to editable buffer directly?
    byte* d=(byte*)data; byte* dend=d+size;
    bool f=true;
    if (bMath) {
      for(;d<dend;d++) {
        if (tkmath[*d]) { byte* s=d++;
          if (*s=='#') { // math tuples
            for (;;d++) { if (d>=dend || me[*d]) { if (*d=='#') { std::cout<<(f?"":" "); f=false; std::cout.write((char*)s,d-s+1); } else { d=s; } break; } }
          } else { // non-math doesn't start with #
            for(;;d++) { if (d>=dend || !tkmath[*d]) { *d=0; std::cout<<(f?"":" ")<<pstem(tolower((char*)s),d-s); f=false; break; } }
          }
        }
      }
    } else {
      for(;d<dend;d++) {
        if (tk[*d]) { cbyte* s=d++;
          //if (d<dend && *(d-1)=='<' && *d=='/') d++; // end tags
          for(;;d++) { if (d>=dend || !tk[*d]) { *d=0; std::cout<<(f?"":" ")<<pstem(tolower((char*)s),d-s); f=false; break; } }
        }
      }
    }
    //for (int i=0;i<tokens.size();i++) { cout<<tokens.at(i)<<" "; } cout<<endl;
    std::cout<<std::endl;
    delete data; data=NULL; // cleanup
  }

  int process(bool bMath) {
    std::istream& in = std::cin;
    std::string line; getline(in,line); if (!in) return 0;
    if (line.compare("<DOC>")!=0) {
      for (;;getline(in,line)) { if (!in) return 0; process(line,bMath); }
    } else {
      NEXTDOC:
      int docHDRLine=0;
      for (;;getline(in,line)) { if (!in) return 0;
        if (docHDRLine<=0) {
          if (line.compare("<DOC>")==0 || line.find("<DOCNO>")==0) { std::cout<<line<<std::endl; }
          else if (line.compare("<DOCHDR>")==0 || docHDRLine>0) { docHDRLine++; }
          else { goto PROCESSLINE; } // no DOCHDR
        } else {
          if (line.compare("</DOCHDR>")==0) { std::cout<<line<<std::endl; break; }
          else if (docHDRLine<=2) std::cout<<line<<std::endl; // pass through non-processed lines, but only first of DocHDR
        }
      }
      for (;;getline(in,line)) { if (!in) return 0;
        PROCESSLINE:
        if (line.compare("</DOC>")==0) { std::cout<<line<<std::endl; goto NEXTDOC; }
        else { process(line,bMath); }
      }
    }
  }
};

static void usage() {std::cerr<<"Usage: ./mtokenize.exe [-M] < in > out"<<std::endl; exit(-1);}

int main(int argc, char *argv[]) {
  MTokenize tokenize; int s=1;
  bool bMath=false;
  if (s<argc && strstr(argv[s],"-M")==argv[s]) { bMath=true; s++; }
  if (argc-s!=0) usage();
  
  return tokenize.process(bMath);
}
