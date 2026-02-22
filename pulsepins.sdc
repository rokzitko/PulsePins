# Primary reference clocks
create_clock -name FPGA_CLK1_50 -period 20.000 [get_ports {FPGA_CLK1_50}]
create_clock -name EXT_CLK      -period 10.000 [get_ports {EXT_CLKp}]

# Derive clocks produced by PLLs / IP
derive_pll_clocks
derive_clock_uncertainty


# ---- ALTCLKCTRL (u_clkctrl) output clock node ----
set STREAMER_MUX_OUT [get_pins *u_clkctrl*outclk*]


# ---- Internal clock source (PLL output that drives int_clk) ----
set INTCLK_SRC [get_pins {u0|pll_int_clk|altera_pll_i|cyclonev_pll|counter[0].output_counter|divclk}]


# streamer_clk when selected from int clock (ALTCLKCTRL inclk[0])
create_generated_clock -name STREAMER_FROM_INT \
  -source $INTCLK_SRC \
  $STREAMER_MUX_OUT

# streamer_clk when selected from external clock (ALTCLKCTRL inclk[1])
create_generated_clock -name STREAMER_FROM_EXT \
  -source [get_ports {EXT_CLKp}] \
  -add \
  $STREAMER_MUX_OUT

# Only one of these is active at a time
set_clock_groups -logically_exclusive \
  -group {STREAMER_FROM_INT} \
  -group {STREAMER_FROM_EXT}


set_false_path -to [get_pins *u_clkctrl*clkselect*]

set_false_path -through [get_pins -compatibility_mode {*pll_reconfig*}]
