// Check public wrapper inputs, including idle controls and unused bus inputs.
always @(negedge clk) begin
  if (reset === 1'b0) begin
    assert(!$isunknown({dut.clk, dut.reset, dut.streamer_clk,
      dut.asi_valid, dut.asi_data, dut.asi_channel, dut.avs_s0_address,
      dut.avs_s0_read, dut.avs_s0_write, dut.avs_s0_writedata,
      dut.trigger_in, dut.trigger_enable_ext, dut.trigger_force_ext,
      dut.trigger_reset_ext, dut.gate_in})) else
      $fatal(1, "unknown or unconnected st_interface input");
  end
end
