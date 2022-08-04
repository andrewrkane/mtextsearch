// (C) Copyright 2019 Andrew R. J. Kane <arkane@uwaterloo.ca>, All Rights Reserved.
//     Released for academic purposes only, All Other Rights Reserved.
//     This software is provided "as is" with no warranties, and the authors are not liable for any damages from its use.

#include <vector>
#include "mdictionary.hpp"

#define STARTTIME(s) std::chrono::high_resolution_clock::time_point s = std::chrono::high_resolution_clock::now();
#define ENDTIME(s,td) td=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-s).count();

// == MDictionary testing ======================================================

static void doTest(istream& in) {
  double td;

  vector<string> v;
  { STARTTIME(s);
    for (int i=0;; i++) { string line; if (!getline(in,line)) { break; } v.push_back(line); } ENDTIME(s,td); }
  int vsize=v.size(); cout<<"data read took "<<td<<"ms size="<<v.size()<<endl;

  //check ordering
  for (int i=1;i<v.size();i++) { cchar* c=v[i].c_str(); cchar* lastc=v[i-1].c_str(); if (strcmp(c,lastc)<0) {cerr<<"ERROR: non-ordered\t"<<c<<"\t"<<lastc<<"\t"<<strcmp(c,lastc)<<endl;exit(-1);} }

  //twolayer list
  { cerr<<"twolayer list"<<endl;
  STARTTIME(s2); ListTwoLayer dlayer;
  { for (int i=0;i<v.size();i++) { dlayer.add(v[i].c_str(),(i==0?NULL:v[i-1].c_str()),0); } dlayer.addEnd(); }
  ENDTIME(s2,td);
  cout <<"dlayer create took "<<td<<"ms " <<td/vsize<<"ms/entry " <<(double)dlayer.memoryusage()/(1<<20)<<"MB "<< (double)dlayer.memoryusage()/vsize<<"b/entry"<<endl;
  { STARTTIME(s);
    for (int i=0;i<vsize;i++) { string line=v[i];
      int t=dlayer.getV(line.c_str()).id; if(t!=i) {cerr<<"ERROR: dlayer get "<<line<<" at i="<<i<<" t="<<t<<endl; exit(-1);} } ENDTIME(s,td); }
  cout <<"dlayer get took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  { STARTTIME(s); cchar* fn="dictionary-test.temp";
    ofstream out(fn); dlayer.write(out); out.close();
    ifstream in(fn); ListTwoLayer dlayer2(in,fn); in.close(); remove(fn);
    if (dlayer.size()!=dlayer2.size()) {cerr<<"ERROR: dlayer2 size "<<dlayer2.size()<<endl; exit(-1);}
    for (int i=0;i<vsize;i++) { string line=v[i];
      int t=dlayer2.getV(line.c_str()).id; if(t!=i) {cerr<<"ERROR: dlayer2 get "<<line<<" at i="<<i<<" t="<<t<<endl; exit(-1);} } ENDTIME(s,td); }
  cout <<"dlayer2 write+read+get took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  }

  //twolayer docnames
  { cerr<<"twolayer docnames"<<endl;
  STARTTIME(s2); DocnamesTwoLayer dlayer;
  { for (uint64_t i=0;i<v.size();i++) { dlayer.add(v[i].c_str(),(i==0?NULL:v[i-1].c_str()),i<<32); } dlayer.addEnd(); }
  ENDTIME(s2,td);
  cout <<"dlayer create took "<<td<<"ms " <<td/vsize<<"ms/entry " <<(double)dlayer.memoryusage()/(1<<20)<<"MB "<< (double)dlayer.memoryusage()/vsize<<"b/entry"<<endl;
  { STARTTIME(s);
    for (int i=0;i<vsize;i++) { string line=v[i];
      uint64_t t=dlayer.getV(line.c_str())>>32; if(t!=i) {cerr<<"ERROR: dlayer getV "<<line<<" failed at i="<<i<<" t="<<t<<endl; exit(-1);} } ENDTIME(s,td); }
  cout <<"dlayer getV took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  { STARTTIME(s);
    const uint cmax=1<<10; char c[cmax];
    for (int i=0;i<vsize;i++) { string line=v[i];
      uint64_t t=dlayer.getV(i)>>32; if(t!=i) {cerr<<"ERROR: dlayer getV(id) "<<line<<" failed at i="<<i<<" t="<<t<<endl; exit(-1);} } ENDTIME(s,td); }
  cout <<"dlayer getV(id) took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  { STARTTIME(s);
    const uint cmax=1<<10; char c[cmax];
    for (int i=0;i<vsize;i++) { string line=v[i];
      dlayer.getK(i,c,cmax); if(line.compare(c)!=0) {cerr<<"ERROR: dlayer getK "<<i<<" "<<line<<" "<<c<<endl; exit(-1);} } ENDTIME(s,td); }
  cout <<"dlayer getK took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  { STARTTIME(s); cchar* fn="dictionary-test.temp";
    ofstream out(fn); dlayer.write(out); out.close();
    ifstream in(fn); DocnamesTwoLayer dlayer2(in,fn); in.close(); remove(fn);
    if (dlayer.size()!=dlayer2.size()) {cerr<<"ERROR: dlayer2 size "<<dlayer2.size()<<endl; exit(-1);}
    const uint cmax=1<<10; char c[cmax];
    for (int i=0;i<vsize;i++) { string line=v[i];
      uint64_t t=dlayer2.getV(line.c_str())>>32; if(t!=i) {cerr<<"ERROR: dlayer2 get "<<line<<" failed at i="<<i<<" return="<<t<<endl; exit(-1);}
      t=dlayer2.getV(i)>>32; if(t!=i) {cerr<<"ERROR: dlayer2 getV(id) "<<line<<" failed at i="<<i<<" t="<<t<<endl; exit(-1);}
      dlayer2.getK(i,c,cmax); if(line.compare(c)!=0) {cerr<<"ERROR: dlayer2 getK "<<i<<" "<<line<<" "<<c<<endl; exit(-1);}
    }
    ENDTIME(s,td); }
    cout <<"dlayer2 write+read+3*get took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  }

  //twolayer dictionary
  { cerr<<"twolayer dictionary"<<endl;
  STARTTIME(s2); DictionaryTwoLayer dlayer;
  { for (uint64_t i=0;i<v.size();i++) { dlayer.add(v[i].c_str(),(i==0?NULL:v[i-1].c_str()),i<<32); } dlayer.addEnd(); }
  ENDTIME(s2,td);
  cout <<"dlayer create took "<<td<<"ms " <<td/vsize<<"ms/entry " <<(double)dlayer.memoryusage()/(1<<20)<<"MB "<< (double)dlayer.memoryusage()/vsize<<"b/entry"<<endl;
  { STARTTIME(s);
    for (int i=0;i<vsize;i++) { string line=v[i];
      uint64_t t=dlayer.getV(line.c_str())>>32; if(t!=i) {cerr<<"ERROR: dlayer get "<<line<<" failed at i="<<i<<" t="<<t<<endl; exit(-1);} } ENDTIME(s,td); }
  cout <<"dlayer get took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  { STARTTIME(s); cchar* fn="dictionary-test.temp";
    ofstream out(fn); dlayer.write(out); out.close();
    ifstream in(fn); DictionaryTwoLayer dlayer2(in,fn); in.close(); remove(fn);
    if (dlayer.size()!=dlayer2.size()) {cerr<<"ERROR: dlayer2 size "<<dlayer2.size()<<endl; exit(-1);}
    for (int i=0;i<vsize;i++) { string line=v[i];
      uint64_t t=dlayer2.getV(line.c_str())>>32; if(t!=i) {cerr<<"ERROR: dlayer2 get "<<line<<" failed at i="<<i<<" return="<<t<<endl; exit(-1);} } ENDTIME(s,td); }
  cout <<"dlayer2 write+read+get took "<<td<<"ms " <<td/vsize<<"ms/entry "<<endl;
  }
}

static void usage() {
  cerr << "Usage: mdictionary.exe < input.txt > output.txt" << endl;
  exit(1);
}

int main(int argc, char **argv) {
  if (argc == 1) {
    doTest(cin);
  } else { usage(); }
}
