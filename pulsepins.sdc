create_clock -period 20.000 [get_ports FPGA_CLK1_50]
create_clock -period 20.000 [get_ports FPGA_CLK2_50]
create_clock -period 20.000 [get_ports FPGA_CLK3_50]
create_clock -name EXT_CLK -period 10.000 [get_ports EXT_CLKp]

derive_pll_clocks
derive_clock_uncertainty

# streamer_clk when selected from int_clk (inclk[0])
create_generated_clock -name STREAMER_FROM_INT \
  -source [get_nets {int_clk}] \
  [get_pins {pulsepins:u_clkctrl|outclk}]

# streamer_clk when selected from EXT_CLKp (inclk[1])
create_generated_clock -name STREAMER_FROM_EXT \
  -source [get_ports {EXT_CLKp}] \
  -add \
  [get_pins {pulsepins:u_clkctrl|outclk}]

set_clock_groups -logically_exclusive \
  -group {STREAMER_FROM_INT} \
  -group {STREAMER_FROM_EXT}

# If sel_clk is not synchronous to either clock, do not time it against clock domains:
set_false_path -to [get_pins {pulsepins:u_clkctrl|clkselect[*]}]

set_false_path -through [get_pins -compatibility_mode {*pll_reconfig*}]
