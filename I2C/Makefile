TARGETHOST ?= de10nano
SCP_TARGET ?= $(TARGETHOST)
IMGROOT ?= ../image/ext/home/root
IMAGE_SUDO ?=

.PHONY: copy

DIR=I2C/
FILES=$(wildcard *.py) README
copy:
	ssh $(SCP_TARGET) mkdir -p $(DIR)
	scp $(FILES) $(SCP_TARGET):$(DIR)

copy_img:
	$(IMAGE_SUDO) mkdir -p $(IMGROOT)/$(DIR)
	$(IMAGE_SUDO) cp -v $(FILES) $(IMGROOT)/$(DIR)
