.PHONY: test test1 test2 test3 test4 test5 test6 clean

test: test1 test2 test3 test4 test5 test6

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

test6:
	./run6
	@[ -f "SUCCESS" ]

clean:
	rm -f tb SUCCESS transcript
	rm -rf work
	rm -rf *.ucdb report.txt report_dir/
