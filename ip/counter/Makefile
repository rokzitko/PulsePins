# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

SUBDIRS := $(wildcard tb_*)

.PHONY: test clean $(SUBDIRS) lint

test: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ test

clean: $(SUBDIRS:%=%-clean)
	rm -rf report.txt report_dir/

$(SUBDIRS:%=%-clean):
	$(MAKE) -C $(@:-clean=) clean

lint-verilator:
	verilator --lint-only -Wall *.sv

lint-verible:
	 verible-verilog-lint --rules=-line-length,-parameter-name-style *.sv

lint: lint-verible
