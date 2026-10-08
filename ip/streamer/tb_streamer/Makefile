.PHONY: test1 test2 test3 test4 test5 test5a test6 test7 test8 test9 test10 test11 test12 test13 clean merge report

all: test1 test2 test3 test4 test5 test5a test6 test7 test8 test9 test10 test11 test12 test13

test1:
	./run1

test2:
	./run2

test3:
	./run3

test4:
	./run4

test5:
	./run5

test5a:
	./run5a

test6:
	./run6

test7:
	./run7

test8:
	./run8

test9:
	./run9

test10:
	./run10

test11:
	./run11

test12:
	./run12

test13:
	./run13

clean:
	rm -f tb SUCCESS transcript
	rm -rf work .sim
	rm -rf *.ucdb report.txt report_dir/
	rm -rf vish_stacktrace.vstf

merge:
	vcover merge merged.ucdb $(wildcard .sim/questa/*/*/coverage.ucdb)

report: merge
	vcover report -annotate -codeAll -details -output report.txt merged.ucdb
	vcover report -annotate -codeAll -details -html -output report_dir merged.ucdb
