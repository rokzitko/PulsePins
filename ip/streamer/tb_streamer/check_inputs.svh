// Check at the DUT boundary so an omitted connection is caught as X/Z too.
always @(negedge clk) begin
  if (reset === 1'b0) begin
    assert(!$isunknown({dut.clk, dut.reset, dut.streamer_clk,
      dut.input_valid, dut.input_data, dut.gate_enable, dut.initial_value,
      dut.initial_reload, dut.initial_value_streamer, dut.trigger_in,
      dut.trigger_enable, dut.trigger_force, dut.trigger_reset,
      dut.stop, dut.stop_on_buffer_error})) else
      $fatal(1, "unknown or unconnected streamer input");
  end
end
