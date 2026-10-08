.PHONY: test test1 test2 test3 test4 test5 test6 clean merge report

test: test1 test2 test3 test4 test5 test6

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

test6:
	./run6

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
