.PHONY: copy

DIR=I2C/
FILES=$(wildcard *.py)
copy:
	ssh $(TARGETHOST) mkdir -p $(DIR)
	scp $(FILES) @$(TARGETHOST):$(DIR)

copy_img:
	mkdir -p $(IMGROOT)/$(DIR)
	cp -v $(FILES) $(IMGROOT)/$(DIR)
