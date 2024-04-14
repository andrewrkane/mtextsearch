// (C) Copyright 2019 Andrew R. J. Kane <arkane (at) uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <stdio.h>
using namespace std;

/* read in TREC files, output only specified DOCs */

enum ProcessMode {accum,drop,output};

static inline string mathDOCNO(bool bMath, string s) { if (bMath) { int c=s.find_last_of('_'); if (c>=0) s=s.substr(c+1); } return s; }

static void process(char* fn, bool bMath) {
  // get docs to extract
  set<string> extract;
  ifstream in(fn); if (!in) {cerr<<"ERROR: invalid "<<fn<<endl; exit(-1);}
  for (;;) { string line; getline(in, line); if (!in) break; extract.insert(mathDOCNO(bMath, line)); }
  cerr<<"Loaded "<<extract.size()<<" DOCNOs for extraction"<<endl;
  // process with tags from stdin
  int curr=0, size=1<<20; char* buff=(char*)malloc(size); ProcessMode m; int extracted=0, processed=0;
  NEXTDOC:
  m=accum; curr=0;
  for (;;) { string line; getline(cin, line); if (!cin) goto CLEANUPBUFF;
    if (line.compare("</DOC>")==0) { processed++; if (m==output) { cout<<line<<endl; extracted++; } goto NEXTDOC; }
    else if (m==drop) continue;
    else if (m==output) { cout<<line<<endl; }
    else if (m==accum) {
      if (line.size()>7 && line.compare(0,7,"<DOCNO>")==0) {
        int e=line.find("</DOCNO>",7);
        if (e>=0) { string docname=mathDOCNO(bMath, line.substr(7,e-7));
          if (extract.find(docname)!=extract.end()) { m=output; cout.write(buff,curr); curr=0; cout<<line<<endl; continue; }
        }
        m=drop; curr=0; continue; // invalid or non-extract DOCNO, so drop
      }
      memcpy(&buff[curr],line.c_str(),line.length()); curr+=line.length(); buff[curr++]='\n'; // accumulate
    }
  }
  CLEANUPBUFF:
  free(buff); buff=NULL;
  cerr<<"Extracted "<<extracted<<" of "<<processed<<" documents"<<endl;
}

int main(int argc, char *argv[]) {
  int s=1; bool bMath=false;
  if (s<argc && strstr(argv[s],"-M")==argv[s] && *(argv[s]+2)==0) { bMath=true; s++; }
  if (argc-s!=1) { cerr<<"Usage: ./mextract.exe [-M] docno_values.txt < input > output"<<endl; return -1; }
  process(argv[s], bMath);
  return 0;
}
