# report_divclk.tcl

# Open project (optional if quartus_sta is called with the project name)
project_open pulsepins

# Build timing netlist
create_timing_netlist
read_sdc
update_timing_netlist

# Option A: use the full clock name if you know it exactly
# set clk_div0 [get_clocks {u0|pll_0|altera_pll_i|cyclonev_pll|counter[0].output_counter|divclk}]

# Option B: use a wildcard pattern (often more robust)
set clk_div0 [get_clocks *counter\[0\].output_counter*divclk*]

# Report the 100 worst paths within this clock domain
report_timing \
    -from $clk_div0 \
    -to   $clk_div0 \
    -npaths 100 \
    -detail full_path \
    -panel_name {divclk_worst_100} \
    -file report_divclk_worst_100.rpt

# Clean up
delete_timing_netlist
project_close
