// Test-only offline sequence expansion. The model has no DUT/state references:
// operations apply once per record, and PRNG state advances per emitted sample.
logic [63:0] reference_words [0:16383];
int reference_count = 0;
logic [31:0] reference_previous;
logic [63:0] reference_s0, reference_s1;
logic [10:0] reference_modes = 0;
int reference_random_samples = 0;

task automatic reference_reset(input logic [31:0] initial_value);
  reference_count = 0;
  reference_previous = initial_value;
  reference_s0 = 64'h0123456789abcdef;
  reference_s1 = 64'hfedcba9876543210;
endtask

function automatic logic [31:0] reference_operation(input int op,
    input logic [31:0] previous, value);
  case (op)
    0: reference_operation = value;
    1, 6: reference_operation = previous | value;
    2: reference_operation = previous & ~value;
    3, 7: reference_operation = previous ^ value;
    4: reference_operation = ~previous;
    5: reference_operation = previous & value;
    8: reference_operation = ~(previous ^ value);
    9: reference_operation = value >= 32 ? 0 : previous * (64'd1 << value);
    10: reference_operation = value >= 32 ? 0 : {32'b0, previous} / (64'd1 << value);
    default: reference_operation = value;
  endcase
endfunction

task automatic reference_run(input logic [31:0] ctrl, count, data);
  logic [31:0] value;
  logic [63:0] mixed;
  int op;
  begin
    op = (ctrl >> BIT_MODE_LO) & 15;
    value = reference_operation(op, reference_previous, data);
    if (count != 0 && op <= 10) reference_modes[op] = 1;
    for (int n = 0; n < count; n++) begin
      if (ctrl[BIT_PRNG]) begin
        value = reference_s0 + reference_s1;
        mixed = reference_s0 ^ reference_s1;
        reference_s0 = {reference_s0[8:0], reference_s0[63:9]} ^ mixed ^ (mixed << 14);
        reference_s1 = {mixed[27:0], mixed[63:28]};
        reference_random_samples++;
      end
      assert(reference_count < 16384) else $fatal(1, "reference sequence too long");
      reference_words[reference_count] = {ctrl, value};
      reference_count++;
      reference_previous = value;
    end
  end
endtask
