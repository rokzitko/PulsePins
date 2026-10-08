.PHONY: test test1 test2 test3 clean merge report

test: test1 test2 test3

test1:
	./run1

test2:
	./run2

test3:
	./run3

clean:
	rm -f tb SUCCESS transcript
	rm -rf work .sim
	rm -rf *.ucdb report.txt report_dir/

merge:
	vcover merge merged.ucdb $(wildcard .sim/questa/*/*/coverage.ucdb)

report: merge
	vcover report merged.ucdb -codeAll -annotate -details -output report.txt
	vcover report -html -annotate -codeAll -output report_dir merged.ucdb
