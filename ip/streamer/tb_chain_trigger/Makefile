.PHONY: test clean

test:
	./run
	@[ -f "SUCCESS" ]

clean:
	rm -f tb SUCCESS transcript
	rm -rf work
	rm -rf *.ucdb report.txt report_dir/
	rm -rf vish_stacktrace.vstf
