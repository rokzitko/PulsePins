SUBDIRS := combiner combiner_comb combiner_trig counter freq_meter misc rl_encoder_if st_mux streamer ts_core
ICARUS_TESTDIRS := counter/tb_autocorrelation counter/tb_basic_counter counter/tb_counter_if \
	counter/tb_crosscorrelation counter/tb_packet_stats counter/tb_runs_counter \
	counter/tb_seq_counter counter/tb_time_counter misc/tb_activity_monitor \
	misc/tb_pulse_gen misc/tb_rand_signal_gen misc/tb_sig_mux misc/tb_tik ts_core/tb
ICARUS_TESTDIRS += combiner/tb_combiner combiner_comb/tb_combiner combiner_trig/tb_combiner \
	freq_meter/tb st_mux/tb_mux misc/tb_cdc misc/tb_cdc_mailbox streamer/tb_and_trigger

.PHONY: test test-icarus clean $(SUBDIRS) $(ICARUS_TESTDIRS:%=%-icarus) decoder-icarus

test: $(SUBDIRS)
	@echo "$(GREEN) SUCCESS $(RESET)"

$(SUBDIRS):
	$(MAKE) -C $@

test-icarus: $(ICARUS_TESTDIRS:%=%-icarus) decoder-icarus

$(ICARUS_TESTDIRS:%=%-icarus):
	$(MAKE) -C $(@:%-icarus=%) SIM=icarus

# Only the standalone decoder scenario in this directory is vendor-independent.
decoder-icarus:
	$(MAKE) -C streamer/tb_streamer SIM=icarus test13

clean: $(SUBDIRS:%=%-clean)

$(SUBDIRS:%=%-clean):
	$(MAKE) -C $(@:-clean=) clean
