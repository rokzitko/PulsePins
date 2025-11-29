// Endianness conversions
// Rok Zitko, 2025

`default_nettype none // turn off implicit data types

module swap_endianness_96
(
  input wire [95:0] in,
  output wire [95:0] out
);

assign out = {
 {in[71:64]},
 {in[79:72]},
 {in[87:80]},
 {in[95:88]},

 {in[39:32]},
 {in[47:40]},
 {in[55:48]},
 {in[63:56]},

 {in[07:00]},
 {in[15:08]},
 {in[23:16]},
 {in[31:24]}
};

endmodule

`default_nettype wire // turn implicit nets on again to avoid side-effects
