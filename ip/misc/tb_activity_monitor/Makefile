.PHONY: test test2 clean

all: test

test: test2
	sh ./compile

test2:
	sh ./compile2

clean:
	rm -rf .sim
	rm -f tb tb_presence_detector.vcd
