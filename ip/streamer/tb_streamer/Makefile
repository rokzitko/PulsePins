.PHONY: test1 test2 test3 test4 test5 test5a test6 clean merge report

all: test1 test2 test3 test4 test5 test5a test6

test1:
	./run1
	@[ -f "SUCCESS" ]

test2:
	./run2
	@[ -f "SUCCESS" ]

test3:
	./run3
	@[ -f "SUCCESS" ]

test4:
	./run4
	@[ -f "SUCCESS" ]

test5:
	./run5
	@[ -f "SUCCESS" ]

test5a:
	./run5a
	@[ -f "SUCCESS" ]

test6:
	./run6
	@[ -f "SUCCESS" ]

clean:
	rm -f tb SUCCESS transcript
	rm -rf work
	rm -rf *.ucdb report.txt report_dir/
	rm -rf vish_stacktrace.vstf

merge:
	vcover merge merged.ucdb $(wildcard run*.ucdb)

report: merge
	vcover report -annotate -codeAll -details -output report.txt merged.ucdb
	vcover report -annotate -codeAll -details -html -output report_dir merged.ucdb
