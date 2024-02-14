SHELL:=/bin/bash

exe=msearch.exe mmerge.exe minvert.exe mstrip.exe mencode.exe mtokenize.exe

all: $(exe)

%.exe: src/%.cpp src/*.hpp
	g++ -o $@ $<

test_%.exe: testsrc/test_%.cpp src/*.hpp
	g++ -Isrc -o $@ $<

# single index + search
test1:
	printf "<DOC>\n<DOCNO>doc1</DOCNO>\nα c <center>b</center>\n</DOC>\n" | ./mstrip.exe | ./minvert.exe > temp_t1.mindex
	./mencode.exe temp_t1.mindex
	echo 'q1; α' | ./msearch.exe temp_t1.mindex
	echo 'q2; b' | ./msearch.exe temp_t1.mindex
	echo 'q3; c c' | ./msearch.exe temp_t1.mindex
	echo 'q4; center' | ./msearch.exe temp_t1.mindex
	rm temp_t[1].mindex*

# double index + merge + search
test2:
	printf "<DOC>\n<DOCNO>doc1</DOCNO>\nα b\n</DOC>\n" | ./minvert.exe > temp_t1.mindex
	printf "<DOC>\n<DOCNO>doc2</DOCNO>\nα c\n</DOC>\n" | ./minvert.exe > temp_t2.mindex
	./mmerge.exe temp_t[12].mindex > temp_t3.mindex
	./mencode.exe temp_t3.mindex
	echo 'q1; α' | ./msearch.exe temp_t3.mindex
	echo 'q2; b' | ./msearch.exe temp_t3.mindex
	echo 'q3; c' | ./msearch.exe temp_t3.mindex
	echo 'q1; α' | ./msearch.exe -k1 temp_t3.mindex
	rm temp_t[123].mindex*

# single index == double index + merge
test3:
	printf "<DOC>\n<DOCNO>doc1</DOCNO>\nα b\n</DOC><DOC>\n<DOCNO>doc2</DOCNO>\nα c\n</DOC>\n" | ./minvert.exe > temp_t1.mindex
	printf "<DOC>\n<DOCNO>doc1</DOCNO>\nα b\n</DOC>\n" | ./minvert.exe > temp_t2.mindex
	printf "<DOC>\n<DOCNO>doc2</DOCNO>\nα c\n</DOC>\n" | ./minvert.exe > temp_t3.mindex
	./mmerge.exe temp_t[23].mindex > temp_t4.mindex
	diff temp_t1.mindex temp_t4.mindex
	rm temp_t[1234].mindex*

# single index == double index + merge; search
test_math:
	printf "<DOC>\n<DOCNO>doc1</DOCNO>\nα b\n #(c)#</DOC><DOC>\n<DOCNO>doc2</DOCNO>\nα c\n #(c)#</DOC>\n" | ./minvert.exe -M > temp_t1.mindex
	printf "<DOC>\n<DOCNO>doc1</DOCNO>\nα b\n #(c)#</DOC>\n" | ./minvert.exe -M > temp_t2.mindex
	printf "<DOC>\n<DOCNO>doc2</DOCNO>\nα c\n #(c)#</DOC>\n" | ./minvert.exe -M > temp_t3.mindex
	./mmerge.exe temp_t[23].mindex > temp_t4.mindex
	diff temp_t1.mindex temp_t4.mindex
	./mencode.exe temp_t1.mindex
	echo 'q1; α' | ./msearch.exe -M temp_t1.mindex
	echo 'q2; b' | ./msearch.exe -M temp_t1.mindex
	echo 'q2; c' | ./msearch.exe -M temp_t1.mindex
	echo 'q2; #(c)#' | ./msearch.exe -k1000 -M -a0.25 temp_t1.mindex
	rm temp_t[1234].mindex*

test_dic: test_mdictionary.exe
	printf "b\nc\nα\nαa\nαα" | sort -u | ./test_mdictionary.exe
	rm test_mdictionary.exe

clean:
	rm $(exe)

