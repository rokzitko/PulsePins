.PHONY: test1 test2 test3 test4 clean

all: test1 test2 test3 test4

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

clean:
	rm -f tb SUCCESS transcript
	rm -rf work
