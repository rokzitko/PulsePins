TARGETHOST ?= de10nano
SCP_TARGET ?= $(TARGETHOST)

.PHONY: copy copy_img

LIST1 = run_all_tests run_all_tests_forever defs.sh
LIST2 = test* sequence.* vcd.*

copy:
	ssh $(SCP_TARGET) "mkdir -p tests"
	scp $(LIST1) $(SCP_TARGET):
	scp $(LIST2) $(SCP_TARGET):tests/

IMGROOT ?= ../image/ext/home/root
IMAGE_SUDO ?=

copy_img:
	$(IMAGE_SUDO) mkdir -p $(IMGROOT)/tests
	$(IMAGE_SUDO) cp -v $(LIST1) $(IMGROOT)
	$(IMAGE_SUDO) cp -v $(LIST2) $(IMGROOT)/tests
