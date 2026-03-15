TARGETHOST ?= de10nano

.PHONY: copy

LIST1 = run_all_tests run_all_tests_forever run_PLL defs.sh
LIST2 = test* sequence.* vcd.*

copy:
	ssh $(TARGETHOST) "mkdir -p tests"
	scp $(LIST1) @$(TARGETHOST):
	scp $(LIST2) @$(TARGETHOST):tests/

IMGROOT=../image/ext/home/root

copy_img:
	mkdir -p $(IMGROOT)/tests
	cp -v $(LIST1) $(IMGROOT)
	cp -v $(LIST2) $(IMGROOT)/tests
