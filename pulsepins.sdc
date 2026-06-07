# Primary reference clocks
create_clock -name FPGA_CLK1_50 -period 20.000 [get_ports {FPGA_CLK1_50}]
create_clock -name EXT_CLK      -period 100.000 [get_ports {EXT_CLKp}]

# HPS peripheral clocks are owned by the hard HPS/peripheral interfaces. They are
# declared so TimeQuest does not treat detected hard-IP clocks as unconstrained.
create_clock -name HPS_I2C1_SCLK  -period 2500.000 [get_ports {HPS_I2C1_SCLK}]
create_clock -name HPS_USB_CLKOUT -period 16.667   [get_ports {HPS_USB_CLKOUT}]

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


# The control, reference/gate, internal streamer, and cleaned external streamer
# domains are independently controlled. Crossings between them are CDC paths
# handled in RTL by synchronizers, FIFOs, Gray counters, or software-stable
# control/readback protocols; they are not closed by setup/hold timing.
set CORE_DOMAIN [require_collection_size CORE_DOMAIN \
  [get_clocks -nowarn {u0|pll_core_clk|altera_pll_i|cyclonev_pll|counter[0].output_counter|divclk}] 1]
set REF_DOMAIN [require_collection_size REF_DOMAIN \
  [get_clocks -nowarn {FPGA_CLK1_50}] 1]
set INT_STREAMER_DOMAIN [require_collection_size INT_STREAMER_DOMAIN \
  [get_clocks -nowarn {u0|pll_int_clk|altera_pll_i|cyclonev_pll|counter[0].output_counter|divclk STREAMER_FROM_INT}] 2]
set CLEAN_STREAMER_DOMAIN [require_collection_size CLEAN_STREAMER_DOMAIN \
  [get_clocks -nowarn {EXT_CLK my_pll|auto_generated|generic_pll1~PLL_OUTPUT_COUNTER|divclk STREAMER_FROM_CLEAN}] 3]
set HPS_PERIPH_DOMAIN [require_collection_size HPS_PERIPH_DOMAIN \
  [get_clocks -nowarn {HPS_I2C1_SCLK HPS_USB_CLKOUT}] 2]

set_clock_groups -asynchronous \
  -group $CORE_DOMAIN \
  -group $REF_DOMAIN \
  -group $INT_STREAMER_DOMAIN \
  -group $CLEAN_STREAMER_DOMAIN \
  -group $HPS_PERIPH_DOMAIN


# Board/user/HPS peripheral ports below have no single board-level setup/hold
# contract to the FPGA timing clocks. Internal paths are still timed after the
# input buffer or before the output buffer; only external port timing is excepted.
set BOARD_ASYNC_INPUT_PORTS [require_collection_size BOARD_ASYNC_INPUT_PORTS \
  [get_ports -nowarn {GPI0AUX[*] GPI0GPIO3 GPI0GPIO10 GPI0GPIO11 GPI0GPIO12 GPI0GPIO13 GPI0GPIO26 GPI0GPIO27 GPI0TRIG[*] GPI1Q[*] HPS_ENET_MDIO HPS_ENET_RX_CLK HPS_ENET_RX_DATA[*] HPS_ENET_RX_DV HPS_I2C1_SDAT HPS_SD_CMD HPS_SD_DATA[*] HPS_UART_RX HPS_USB_CLKOUT HPS_USB_DATA[*] HPS_USB_DIR HPS_USB_NXT KEY[*] PPS_IN SW[*]}] 87]
set BOARD_ASYNC_OUTPUT_PORTS [require_collection_size BOARD_ASYNC_OUTPUT_PORTS \
  [get_ports -nowarn {ARDUINO_IO[0] ARDUINO_IO[1] ARDUINO_IO[2] ARDUINO_IO[3] ARDUINO_IO[4] ARDUINO_IO[5] ARDUINO_IO[6] ARDUINO_IO[7] ARDUINO_IO[8] ARDUINO_IO[9] ARDUINO_IO[10] ARDUINO_IO[11] ARDUINO_IO[12] ARDUINO_IO[13] GPI0AUX[*] GPI0GPIO0 GPI0GPIO1 GPI0GPIO2 GPI0GPIO3 GPI0GPIO4 GPI0GPIO5 GPI0GPIO6 GPI0GPIO7 GPI0GPIO8 GPI0GPIO9 GPI0GPIO22 GPI0GPIO23 GPI0GPIO26 GPI0GPIO27 GPI1Q[*] HPS_ENET_GTX_CLK HPS_ENET_MDC HPS_ENET_MDIO HPS_ENET_TX_DATA[*] HPS_ENET_TX_EN HPS_I2C1_SCLK HPS_I2C1_SDAT HPS_SD_CLK HPS_SD_CMD HPS_SD_DATA[*] HPS_UART_TX HPS_USB_DATA[*] HPS_USB_STP LED[*]}] 102]

set_false_path -from $BOARD_ASYNC_INPUT_PORTS
set_false_path -to $BOARD_ASYNC_OUTPUT_PORTS


set STREAMER_CLKSELECT [require_collection_size STREAMER_CLKSELECT \
  [get_pins -nowarn {u_clkctrl|auto_generated|sd2|clkselect[0] u_clkctrl|auto_generated|sd2|clkselect[1]}] 2]
set_false_path -to $STREAMER_CLKSELECT

set_false_path -through [get_pins -compatibility_mode {*pll_reconfig*}]
