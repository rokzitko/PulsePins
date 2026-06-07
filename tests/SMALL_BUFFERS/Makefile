TARGETHOST ?= de10nano
SCP_TARGET ?= $(TARGETHOST)

.PHONY: copy

copy:
	ssh $(SCP_TARGET) "mkdir -p tests"
	scp test* sequence.* $(SCP_TARGET):tests/
