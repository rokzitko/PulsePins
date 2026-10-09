.PHONY: test test2 clean

all: test

test: test2
	sh ./compile

test2:
	sh ./compile2

clean:
	rm -rf .sim
	rm -f tb rand_signal_gen.vcd
