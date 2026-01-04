TARGETHOST ?= de10nano

.PHONY: copy

copy:
	ssh $(TARGETHOST) "mkdir -p tests"
	scp test* sequence.* @$(TARGETHOST):tests/
