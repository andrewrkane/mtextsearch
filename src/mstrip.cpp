// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//   Released for academic purposes only, All Other Rights Reserved.
//   This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <fstream>
#include "mtrec.hpp"

/* read in TREC files, strip html from DOC and output in TREC format */

static bool pr[256]; // process
void setupPR() {
  for (int i=0;i<256;i++) pr[i]=false;
  pr['<']=pr['&']=pr['\r']=pr['\t']=pr['\v']=pr['\f']=pr['\n']=pr[' ']=true;
}

#define REMAIN_2(s) i+2<size && data[i+1]==s[0] && data[i+2]==s[1]
#define REMAIN_3(s) i+3<size && data[i+1]==s[0] && data[i+2]==s[1] && data[i+3]==s[2]

#define REMAIN_5lower(s) i+5<size && tolower(data[i+1])==s[0] && tolower(data[i+2])==s[1] && tolower(data[i+3])==s[2] && tolower(data[i+4])==s[3] && tolower(data[i+5])==s[4]

#define REMAIN_6lower(s) i+6<size && tolower(data[i+1])==s[0] && tolower(data[i+2])==s[1] && tolower(data[i+3])==s[2] && tolower(data[i+4])==s[3] && tolower(data[i+5])==s[4] && tolower(data[i+6])==s[5]

#define REMAIN_7lower(s) i+7<size && tolower(data[i+1])==s[0] && tolower(data[i+2])==s[1] && tolower(data[i+3])==s[2] && tolower(data[i+4])==s[3] && tolower(data[i+5])==s[4] && tolower(data[i+6])==s[5] && tolower(data[i+7])==s[6]

#define REMAIN_8lower(s) i+8<size && tolower(data[i+1])==s[0] && tolower(data[i+2])==s[1] && tolower(data[i+3])==s[2] && tolower(data[i+4])==s[3] && tolower(data[i+5])==s[4] && tolower(data[i+6])==s[5] && tolower(data[i+7])==s[6] && tolower(data[i+8])==s[7]

void strip(/*in/out*/ char*& outd, /*in/modified*/ char* data, int size, char wspace=0) {
  // write modifications into outd
  for (int i=0;i<size;i++) {
    if (!pr[(unsigned char)data[i]]) { if (wspace!=0) { *outd++=wspace; wspace=0; } *outd++=data[i]; continue; }
    unsigned char c=data[i];
    switch (c) {
      case '<':
        // html comment
        if (REMAIN_3("!--")) { for (i+=4;i<size;i++) { if (data[i]=='-'&& REMAIN_2("->")) { i+=2; break; } } c=' '; }
        // script tag
        else if (REMAIN_6lower("script")) { for (i+=7;i<size;i++) { if (data[i]=='<'&& REMAIN_8lower("/script>")) { i+=8; break; } } c=' '; }
        // style tag
        else if (REMAIN_5lower("style")) { for (i+=6;i<size;i++) { if (data[i]=='<'&& REMAIN_7lower("/style>")) { i+=7; break; } } c=' '; }
        // other tags
        else { for (i++;i<size;i++) { if (data[i]=='>') { break; } } c=' '; }
        break;
      case '&':
        // html &nbsp; escaping
        for (int k=i+1;k<size;k++) { if (data[k]==';') { i=k; c=' '; break; } else if (isspace(data[k])) break; }
        break;
      case '\r': case '\t': case '\v': case '\f': //case '\n':
        c=' '; break;
    }
    if (c==' ') { if (wspace!='\n') wspace=c; } // collapse whitespace, newline takes precedence
    else if (c=='\n') { wspace=c; }
    else { if (wspace!=0) { *outd++=wspace; wspace=0; } *outd++=c; }
  }
  if (wspace!=0) { *outd++=wspace; wspace=0; }
}

class MStripCB : public MTREC {
  void processTREC(/*in/edited*/ char* sdata, char* edata, char* sdocno, char* edocno, char* sdochdr, char* edochdr, char* scontent, char* econtent) {
    // write into data array, update pointers so can call next processTREC in chain
    char* e=scontent;
    if (sdochdr!=edochdr) {
      // only output first line of DOCHDR
      e=sdochdr; if (*e=='\n') e++; for (;e<edochdr;e++) { if (*e=='\n') { e++; break; } }
      int l=scontent-edochdr; memcpy(e,edochdr,l); edochdr=e; e+=l; //</DOCHDR>
    }
    strip(e,scontent,econtent-scontent); // modify data
    int l=edata-econtent; memcpy(e,econtent,edata-econtent); econtent=e; e+=l; //</DOC>
    edata=e;
    fwrite(sdata,sizeof(char),e-sdata,stdout);
  }
};

static void usage() {
  std::cerr<<"Usage: ./mstrip.exe datafile ... "<<std::endl;
  std::cerr<<"       ./mstrip.exe < datafile"<<std::endl;
}

int main(int argc, char *argv[]) {
  MStripCB cb; int s=1;
  setupPR();
  if (argc-s==0) { doTREC(std::cin,"stdin",cb); } else if (argc-s>0) { for (;s<argc;s++) { std::ifstream in(argv[s]); if (!in) usage(); doTREC(in,argv[s],cb); } } else usage(); // input
  return 0;
}
