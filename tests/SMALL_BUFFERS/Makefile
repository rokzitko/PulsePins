TARGETHOST ?= de10nano
SCP_TARGET ?= $(TARGETHOST)

.PHONY: copy

copy:
	ssh $(SCP_TARGET) "mkdir -p tests/SMALL_BUFFERS/"
	scp test* $(SCP_TARGET):tests/SMALL_BUFFERS/
