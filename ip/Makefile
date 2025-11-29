SUBDIRS := combiner combiner_comb combiner_trig counter rl_encoder_if st_mux streamer ts_core

.PHONY: test clean $(SUBDIRS)

test: $(SUBDIRS)
	@echo "$(GREEN) SUCCESS $(RESET)"

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(SUBDIRS:%=%-clean)

$(SUBDIRS:%=%-clean):
	$(MAKE) -C $(@:-clean=) clean
