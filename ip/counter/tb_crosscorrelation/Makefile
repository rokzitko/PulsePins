.PHONY: test test1 test2 clean

test: test1 test2

test1:
	sh ./compile1
	vvp ./tb1

test2:
	sh ./compile2
	vvp ./tb2

clean:
	rm -f tb1 tb2
