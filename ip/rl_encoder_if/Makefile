# The test infrastructure is designed for QuartaSim.
# Free version is fine, but the license needs to be requested every year.
# Rok Zitko, 2025

SUBDIRS := $(wildcard tb_*)

.PHONY: test clean $(SUBDIRS)

GREEN := \033[0;32m
RESET := \033[0m

test: $(SUBDIRS)
	@echo "$(GREEN) SUCCESS $(RESET)"

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(SUBDIRS:%=%-clean)

$(SUBDIRS:%=%-clean):
	$(MAKE) -C $(@:-clean=) clean

lint:
	verilator --lint-only -Wall *.sv
