# mtextsearch
BM25 search engine - Goals: small code, good performance

Currently the system supports only TEXT input, terms are normalized (case folding and porter stemmer), ranking is simple (basic BM25).


LICENSES:
The porter stemmer code is from http://www.tartarus.org/~martin/PorterStemmer
The other code is (C) Copyright as specified in each file.


COMPONENTS:
(the makefile contains execution examples)

mstrip (fast) - removes tags and comments from content (trecdoc)

minvert (fast) - indexes/inverts input (trecdoc), outputs variable-byte index (mindex)

mmerge (fast) - combines multiple mindex files

mencode (fast) - loads mindex file, outputs fast loading dictionary structures pointing into mindex file (mindex.meta)

msearch (fast loading, slow queries via exhaustive-OR) - loads mindex and mindex.meta pair of files, runs queries and outputs (-k#) results, post processing can convert to trec format

