# Primary reference clocks
create_clock -name FPGA_CLK1_50 -period 20.000 [get_ports {FPGA_CLK1_50}]
create_clock -name EXT_CLK      -period 100.000 [get_ports {EXT_CLKp}]

# Derive clocks produced by PLLs / IP
derive_pll_clocks
derive_clock_uncertainty

proc require_collection_size {label collection expected} {
  set count [get_collection_size $collection]
  if {$count != $expected} {
    error "$label matched $count object(s), expected $expected. Update pulsepins.sdc for the current post-map TimeQuest names."
  }
  return $collection
}

# ---- ALTCLKCTRL (u_clkctrl) output clock node ----
set STREAMER_MUX_OUT [require_collection_size STREAMER_MUX_OUT \
  [get_pins -nowarn {u_clkctrl|auto_generated|sd1|outclk}] 1]


# ---- Streamer clock sources ----
set INTCLK_SRC [require_collection_size INTCLK_SRC \
  [get_pins -nowarn {u0|pll_int_clk|altera_pll_i|cyclonev_pll|counter[0].output_counter|divclk}] 1]
set CLEANCLK_SRC [require_collection_size CLEANCLK_SRC \
  [get_pins -nowarn {my_pll|auto_generated|generic_pll1~PLL_OUTPUT_COUNTER|divclk}] 1]


# streamer_clk when selected from the internal PLL clock.
create_generated_clock -name STREAMER_FROM_INT \
  -source $INTCLK_SRC \
  $STREAMER_MUX_OUT

# streamer_clk when selected from the cleaned external-clock PLL output.
create_generated_clock -name STREAMER_FROM_CLEAN \
  -source $CLEANCLK_SRC \
  -add \
  $STREAMER_MUX_OUT

# Only one streamer clock source is active at a time.
set_clock_groups -logically_exclusive \
  -group [get_clocks {STREAMER_FROM_INT}] \
  -group [get_clocks {STREAMER_FROM_CLEAN}]


set STREAMER_CLKSELECT [require_collection_size STREAMER_CLKSELECT \
  [get_pins -nowarn {u_clkctrl|auto_generated|sd2|clkselect[0] u_clkctrl|auto_generated|sd2|clkselect[1]}] 2]
set_false_path -to $STREAMER_CLKSELECT

set_false_path -through [get_pins -compatibility_mode {*pll_reconfig*}]
