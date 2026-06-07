TARGETHOST ?= de10nano
SCP_TARGET ?= $(TARGETHOST)

.PHONY: copy copy_img

LIST1 = run_all_tests run_all_tests_forever run_PLL defs.sh
LIST2 = test* sequence.* vcd.*

copy:
	ssh $(SCP_TARGET) "mkdir -p tests"
	scp $(LIST1) $(SCP_TARGET):
	scp $(LIST2) $(SCP_TARGET):tests/

IMGROOT=../image/ext/home/root

copy_img:
	mkdir -p $(IMGROOT)/tests
	cp -v $(LIST1) $(IMGROOT)
	cp -v $(LIST2) $(IMGROOT)/tests
