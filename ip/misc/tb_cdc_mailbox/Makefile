.PHONY: clean

all: test

test:
	./run
	@[ -f "SUCCESS" ]

clean:
	rm -f tb SUCCESS tb.vcd transcript
	rm -rf work
	rm -rf *.ucdb report.txt report_dir/
