// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <stdio.h>

/* read in TREC files, strip html from DOC and output in TREC format */

static bool pr[256];
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

void process(FILE* out, /*in*/ const char* data, int size, char whitespace=0) {
  for (int i=0;i<size;i++) {
    unsigned char c=data[i];
    if (!pr[c]) { if (whitespace!=0) { putc(whitespace,out); whitespace=0; } putc(c,out); continue; }
    switch (c) {
      case '<':
        // html comment
        if (REMAIN_3("!--")) { for (i+=4;i<size;i++) { c=data[i]; if (c=='-'&& REMAIN_2("->")) { i+=2; break; } } c=' '; }
        // script tag
        else if (REMAIN_6lower("script")) { for (i+=7;i<size;i++) { c=data[i]; if (c=='<'&& REMAIN_8lower("/script>")) { i+=8; break; } } c=' '; }
        // style tag
        else if (REMAIN_5lower("style")) { for (i+=6;i<size;i++) { c=data[i]; if (c=='<'&& REMAIN_7lower("/style>")) { i+=7; break; } } c=' '; }
        // other tags
        else { for (i++;i<size;i++) { c=data[i]; if (c=='>') { break; } } c=' '; }
        break;
      case '&':
        // html &nbsp; escaping
        for (int k=i+1;k<size;k++) { if (data[k]==';') { i=k; c=' '; break; } else if (isspace(data[k])) break; }
        break;
      case '\r': case '\t': case '\v': case '\f': //case '\n':
        c=' '; break;
    }
    if (c==' ') { if (whitespace!='\n') whitespace=c; } // collapse whitespace, newline takes precedence
    else if (c=='\n') { whitespace=c; }
    else { if (whitespace!=0) { putc(whitespace,out); whitespace=0; } putc(c,out); }
  }
}

int main(int argc, char *argv[]) {
  setupPR();
  // process all input without DOC,DOCNO,DOCHDR tags
  if (argc==2 && strstr(argv[1],"-x")==argv[1]) {
    std::ostringstream buffer; buffer<<std::cin.rdbuf(); // read all
    std::string data = buffer.str(); process(stdout,data.c_str(),data.length()); std::cout<<std::endl; // process
    return 0;
  }
  if (argc==2 && strstr(argv[1],"-q")==argv[1]) {
    for (bool bfirst=true;;) { std::string line; getline(std::cin, line); if (!std::cin) break;
      int tstart=line.rfind("Query topic=\"");
      if (tstart>=0) { tstart+=13;
        int tend=line.find("\"",tstart);
        if (tend>=0) { if (bfirst) bfirst=false; else std::cout<<std::endl; std::cout<<line.substr(tstart,tend-tstart)<<";"; continue; }
      }
      process(stdout,line.c_str(),line.length(),(tstart>=0?0:' '));
    } std::cout<<std::endl;
    return 0;
  }
  // process with tags
  if (argc!=1) { std::cerr<<"Usage: ./mstrip.exe [-x|-q] < input > output"<<std::endl<<"   where -x = no DOC tags, -q = math query file"; return -1; }
  // data from stdin
  int curr=0, size=1<<20; char* buff=(char*)malloc(size);
  NEXTDOC:
  std::string line;
  int docHDRLine=0;
  for (;;) { getline(std::cin, line); if (!std::cin) goto CLEANUPBUFF;
    if (docHDRLine<=0) {
      if (line.compare("<DOC>")==0 || line.find("<DOCNO>")==0) { std::cout<<line<<std::endl; }
      else if (line.compare("<DOCHDR>")==0 || docHDRLine>0) { docHDRLine++; std::cout<<line<<std::endl; }
      else { goto PROCESSLINE; } // no DOCHDR
    } else {
      if (line.compare("</DOCHDR>")==0) { std::cout<<line<<std::endl; break; }
      else if (docHDRLine<2) { docHDRLine++; std::cout<<line<<std::endl; } // pass through non-processed lines, but only first of DocHDR
    }
  }
  for (;;) { getline(std::cin, line); if (!std::cin) goto CLEANUPBUFF;
    PROCESSLINE:
    if (line.compare("</DOC>")==0) {
      process(stdout,buff,curr); std::cout<<std::endl; curr=0; // process accumulated at end of doc
      std::cout<<line<<std::endl; // pass through non-processed lines
      goto NEXTDOC;
    } else {
      int len=line.length();
      while (curr+len+1>size) { size*=2; buff=(char*)realloc(buff,size); } // grow
      memcpy(&buff[curr],line.c_str(),len); curr+=len; buff[curr++]='\n'; // accumulate
    }
  }
  CLEANUPBUFF:
  free(buff); buff=NULL;
  return 0;
}
