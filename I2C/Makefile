TARGETHOST ?= de10nano
SCP_TARGET ?= $(TARGETHOST)
IMGROOT ?= ../image/ext/home/root

.PHONY: copy

DIR=I2C/
FILES=$(wildcard *.py) README
copy:
	ssh $(SCP_TARGET) mkdir -p $(DIR)
	scp $(FILES) $(SCP_TARGET):$(DIR)

copy_img:
	mkdir -p $(IMGROOT)/$(DIR)
	cp -v $(FILES) $(IMGROOT)/$(DIR)
