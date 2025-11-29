.PHONY: test clean merge report

test: test1 test2 test3

test1:
	./run1
	@[ -f "SUCCESS" ]

test2:
	./run2
	@[ -f "SUCCESS" ]

test3:
	./run3
	@[ -f "SUCCESS" ]

clean:
	rm -f tb SUCCESS transcript
	rm -rf work
	rm -rf *.ucdb report.txt report_dir/

merge:
	vcover merge merged.ucdb run.ucdb

report: merge
	vcover report merged.ucdb -codeAll -annotate -details -output report.txt
	vcover report -html -annotate -codeAll -output report_dir merged.ucdb
