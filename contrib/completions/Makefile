TARGETHOST ?= de10nano

SOURCE = pulsepins.bash
TARGET_DIR = /etc/profile.d
TARGET_FILE = $(TARGET_DIR)/pulsepins-completion.sh
IMGROOT = ../../image
IMG_TARGET_DIR = $(IMGROOT)/etc/profile.d
IMG_TARGET_FILE = $(IMG_TARGET_DIR)/pulsepins-completion.sh

.PHONY: copy copy_img

copy:
	ssh $(TARGETHOST) mkdir -p $(TARGET_DIR)
	scp $(SOURCE) @$(TARGETHOST):$(TARGET_FILE)
	ssh $(TARGETHOST) chmod 0644 $(TARGET_FILE)

copy_img:
	mkdir -p $(IMG_TARGET_DIR)
	cp -v $(SOURCE) $(IMG_TARGET_FILE)
	chmod 0644 $(IMG_TARGET_FILE)
