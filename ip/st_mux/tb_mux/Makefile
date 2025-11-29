.PHONY: clean

all: test1

test1:
	./run1
	@[ -f "SUCCESS" ]
clean:
	rm -f tb SUCCESS transcript
	rm -rf work
	rm -rf *.ucdb report.txt report_dir/
