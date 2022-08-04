// (C) Copyright 2019 Andrew R. J. Kane <arkane@uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.
// project: https://github.com/andrewrkane/mtextsearch

#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>

#include "mdictionary.hpp"
using namespace std;

/* read in mindex file, output dict file pointing into it */

class MEncode { protected:
  DocnamesTwoLayer docs; uint64_t totaltokens;
  DictionaryTwoLayer dict;
  void inputPostings(ifstream& in, const char* fn) {
    int count=0; string line; string lasttoken="";
    for (;;) {
      // name+size
      uint64_t loc=in.tellg();
      string token; in>>token; if (token.compare("")==0) break;
      int blen; in>>blen;
      getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: index "<<fn<<" postings info "<<token<<endl; exit(-1);}
      // postings
      cchar* c=token.c_str(); cchar* lastc=lasttoken.c_str();
      //if ((lastc[0]!=0) && (strcmp(c,lastc)<0)) {cerr<<"ERROR: non-ordered\t"<<c<<"\t"<<lastc<<"\t"<<strcmp(c,lastc)<<endl;exit(-1);}
      dict.add(c,lastc,loc); // point to (token \t bytelength \n data)
      lasttoken=token;
      in.ignore(blen);
      getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: index "<<fn<<" postings "<<token<<endl; exit(-1);}
      count++;
    }
    cerr<<"Input "<<count<<" postings lists."<<endl;
    dict.addEnd();
    cerr<<"dictionary "<<dict.memoryusage()<<" bytes "<<(double)dict.memoryusage()/count<<" b/entry"<<endl;
  }
public:
  MEncode() { totaltokens=0L; }
  void input(const char* fn) {
    ifstream in(fn); string line; string lastdoc="";
    if (!in.is_open()) {cerr<<"ERROR: Could not open input file "<<fn<<endl; exit(-1);}
    // decide what type of file
    getline(in,line); if (!in) {cerr<<"ERROR: Empty input file "<<fn<<endl; exit(-1);}
    if (line.compare("text.mindex.1")!=0) {cerr<<"ERROR: Unknown format "<<fn<<" found "<<line<<endl; exit(-1);}
    // doccount
    int doccount; in>>doccount;
    getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: index "<<fn<<"doccount info "<<line<<endl; exit(-1);}
    // size+docnames
    for (int i=0;i<doccount;i++) {
      int docsize; in>>docsize; in>>std::ws; getline(in,line);
      docs.add(line.c_str(),lastdoc.c_str(),docsize); totaltokens+=docsize; lastdoc=line;
    }
    docs.addEnd();
    getline(in,line); if (line.compare("")!=0) {cerr<<"ERROR: index "<<fn<<" document names "<<line<<endl;}
    cerr<<"Input "<<doccount<<" document names."<<endl;
    // postings
    inputPostings(in,fn);
    // write
    string metafn=(string)fn+".meta"; ofstream out(metafn);
    struct stat sb; int er=stat(fn,&sb); uint64_t fsize=(uint64_t)sb.st_size; out<<fsize<<endl; // index file size to ensure correct pairing
    docs.write(out); out<<totaltokens<<endl; dict.write(out); out.close();
  }
};

static void usage() {cerr<<"Usage: ./mencode.exe data.mindex"<<endl; exit(-1);}

int main(int argc, char *argv[]) {
  if (argc!=2) usage();
  cerr<<"Input "<<argv[1]<<endl;
  MEncode ms; ms.input(argv[1]); // from mindex
  cerr<<"Done input."<<endl;
  return 0;
}
