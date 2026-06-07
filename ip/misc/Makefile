# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

SUBDIRS := $(wildcard tb_*)

.PHONY: test clean $(SUBDIRS)

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
