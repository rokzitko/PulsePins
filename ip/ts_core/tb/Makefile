.PHONY: test test1 clean

all: test

test: test1

test1:
	sh ./compile1
	vvp ./tb1
	@[ -f "SUCCESS" ]

clean:
	rm -f tb1 SUCCESS
