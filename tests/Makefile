TARGETHOST ?= de10nano

.PHONY: copy

copy:
	ssh $(TARGETHOST) "mkdir -p tests"
	scp run_all_tests run_all_tests_forever run_PLL defs.sh @$(TARGETHOST):
	scp test* sequence.* @$(TARGETHOST):tests/

IMGROOT=../image/ext/home/root

copy_img:
	mkdir -p $(IMGROOT)/tests
	cp -v run_all_tests run_all_tests_forever run_PLL $(IMGROOT)
	cp -v test* sequence.* $(IMGROOT)/tests
