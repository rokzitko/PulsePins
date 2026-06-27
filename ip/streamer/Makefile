# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

# The test infrastructure is designed for QuartaSim.
# Free version is fine, but the license needs to be requested every year.

SUBDIRS := $(wildcard tb_*)

.PHONY: test clean $(SUBDIRS) lint

GREEN := \033[0;32m
RESET := \033[0m

test: $(SUBDIRS)
	@echo "$(GREEN) SUCCESS $(RESET)"

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(SUBDIRS:%=%-clean)
	rm -rf report.txt report_dir/

$(SUBDIRS:%=%-clean):
	$(MAKE) -C $(@:-clean=) clean

lint-verilator:
	verilator --lint-only -Wall *.sv

lint-verible:
	 verible-verilog-lint --rules=-line-length,-parameter-name-style *.sv

lint: lint-verible

merge:
	vcover merge merged.ucdb $(wildcard tb_*/run*.ucdb)

report: merge
	vcover report -annotate -codeAll -details -cvg -assert -output report.txt merged.ucdb
	vcover report -annotate -codeAll -details -cvg -assert -html -output report_dir merged.ucdb

SITE ?= auger:/var/www/html/pulsepins/

upload: report
	scp -r report_dir ${SITE}
