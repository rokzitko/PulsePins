`timescale 1ns/1ps

// Icarus uses a shared $urandom generator. Seed it before any clocked stimulus;
// Questa instead initializes the process hierarchy through -sv_seed.
module tb_runtime;
  integer seed;
  integer unused;
  initial begin
    if (!$value$plusargs("seed=%d", seed)) seed = 1;
    unused = $urandom(seed);
    if ($test$plusargs("waves")) begin
      $dumpfile("waves.vcd");
      $dumpvars(0);
    end
  end
endmodule
