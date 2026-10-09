// Include inside a testbench module. Report only observed activity or completed
// checks, and call every required bin before SUCCESS/$finish. See README.md.
task automatic tb_require_scenario(input string name,
                                   input longint observed,
                                   input longint minimum = 1);
  $display("SCENARIO %s %0d %0d", name, observed, minimum);
  if ((minimum > 0 && observed >= minimum) !== 1'b1)
    $fatal(1, "missing required scenario %s: observed=%0d required=%0d", name, observed, minimum);
endtask
