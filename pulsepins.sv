// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// PulsePins, Rok Zitko, 2020-2026

// Parts derived from rsyocto (c) Robin Sebastian (https://github.com/robseb/rsyocto).
// Licensed under the MIT license.

`default_nettype none

// Uncomment one (and only one) of the following:
//`define INTERNAL_CLK
//`define EXTERNAL_CLK
//`define EXTERNAL_CLK_CLEAN
//`define SELECT_CLK
`define SELECT_CLK_CLEAN

`define WIDTH_DATA         32
`define WIDTH_TRIGGER      8
`define WIDTH_TRIGGER_ALL  (`WIDTH_TRIGGER+3)
`define POS_TRIG_ENABLE    (`WIDTH_TRIGGER)
`define POS_TRIG_FORCE     (`WIDTH_TRIGGER+1)
`define POS_TRIG_RESET     (`WIDTH_TRIGGER+2)
`define WIDTH_AUX          8
`define WIDTH_CFG          13

parameter TS_SIGA_MUX_NR_IN = 8;
parameter TS_SIGA_MUX_WIDTH = $clog2(TS_SIGA_MUX_NR_IN);

`define CFG_OE                     0
`define CFG_TS_SEL_PPS             1
`define CFG_TS_SEL_SIGA_MUX_OFFSET 2
`define CFG_AUX_DIR_OFFSET         (2+TS_SIGA_MUX_WIDTH)

`define ARDUINO_DEBUG_PORT

module pulsepins(
`ifdef ARDUINO_DEBUG_PORT
    output          [15:0]      ARDUINO_IO,
`endif
    output          [14:0]      HPS_DDR3_ADDR,
    output           [2:0]      HPS_DDR3_BA,
    output                      HPS_DDR3_CAS_N,
    output                      HPS_DDR3_CKE,
    output                      HPS_DDR3_CK_N,
    output                      HPS_DDR3_CK_P,
    output                      HPS_DDR3_CS_N,
    output           [3:0]      HPS_DDR3_DM,
    inout           [31:0]      HPS_DDR3_DQ,
    inout            [3:0]      HPS_DDR3_DQS_N,
    inout            [3:0]      HPS_DDR3_DQS_P,
    output                      HPS_DDR3_ODT,
    output                      HPS_DDR3_RAS_N,
    output                      HPS_DDR3_RESET_N,
    input                       HPS_DDR3_RZQ,
    output                      HPS_DDR3_WE_N,
    output                      HPS_ENET_GTX_CLK,
    inout                       HPS_ENET_INT_N,
    output                      HPS_ENET_MDC,
    inout                       HPS_ENET_MDIO,
    input                       HPS_ENET_RX_CLK,
    input            [3:0]      HPS_ENET_RX_DATA,
    input                       HPS_ENET_RX_DV,
    output           [3:0]      HPS_ENET_TX_DATA,
    output                      HPS_ENET_TX_EN,
    inout                       HPS_I2C1_SCLK,
    inout                       HPS_I2C1_SDAT,
    inout                       HPS_KEY,
    inout                       HPS_LED,
    output                      HPS_SD_CLK,
    inout                       HPS_SD_CMD,
    inout            [3:0]      HPS_SD_DATA,
    input                       HPS_UART_RX,
    output                      HPS_UART_TX,
    input                       HPS_USB_CLKOUT,
    inout            [7:0]      HPS_USB_DATA,
    input                       HPS_USB_DIR,
    input                       HPS_USB_NXT,
    output                      HPS_USB_STP,
    inout                       GPI0GPIO0,  // streamer_qout_strobe
    output                      GPI0GPIO1,  // oe
    output                      GPI0GPIO2,  // streamer_clk
    inout                       GPI0GPIO3,  // streamer_qout_valid
    output                      GPI0GPIO4,  // activity
    output                      GPI0GPIO5,  // heartbeat
    output                      GPI0GPIO6,  // streamer_trigger_armed
    output                      GPI0GPIO7,  // streamer_trigger_activated
    output                      GPI0GPIO8,  // streamer_done
    output                      GPI0GPIO9,  // streamer_buffer_error
    input                       GPI0GPIO10, // ext_trigger_enable
    input                       GPI0GPIO11, // ext_trigger_force
    input                       GPI0GPIO12, // ext_trigger_reset
    input                       GPI0GPIO13, // gate_in
    input [`WIDTH_TRIGGER-1:0]  GPI0TRIG,   // ext_trigger_in
    output                      GPI0GPIO22, // extra (setA: streamer_trigger_enable)
    output                      GPI0GPIO23, // extra (setA: streamer_trigger_force)
    output                      GPI0GPIO24, // extra (setA: streamer_trigger_reset)
    output                      GPI0GPIO25, // extra (setA: streamer_trigger_in[0])
    inout                       GPI0GPIO26, // SCL
    inout                       GPI0GPIO27, // SDA
    inout  [`WIDTH_AUX-1:0]     GPI0AUX,    // AUX I/O
    input                       EXT_CLKp,   // GPI1GPIO[0]
    input                       PPS_IN,     // GPI1GPIO[1]
//  input                       PPCLK1,     // GPI1GPIO[2] (for future use)
//  input                       PPCLK2,     // GPI1GPIO[3] (for future use)
    inout            [31:0]     GPI1Q,      // q[0]..q[31], GPI1GPIO[4]..GPI1GPIO[35]
    input            [1:0]      KEY,
    output           [7:0]      LED,
    input            [3:0]      SW,
    input                       FPGA_CLK1_50
//  input                       FPGA_CLK2_50, // not used
//  input                       FPGA_CLK3_50  // not used
);

logic [`WIDTH_TRIGGER_ALL-1:0] pio_trig_int; // output PIO; triggering from software
logic [31:0] pio_trig_monitor;               // input PIO; read values of trigger signals
logic [`WIDTH_AUX-1:0] pio_aux_in;           // i/o PIO; auxiliary inputs
logic [`WIDTH_AUX-1:0] pio_aux_out;          // i/o PIO; auxiliary outputs
logic [31:0] pio_elapsed;                    // input PIO; time since last reset
logic [`WIDTH_CFG-1:0] pio_cfg;              // output PIO; output enable, time stamp select, etc.

logic scl1_o, scl1_o_e;
logic sda1_o, sda1_o_e;

logic core_clk_pll_locked; // locked signal from PLL that generates core_clk
logic int_clk_pll_locked;  // locked signal from PLL that generates int_clk

logic ref_clk;      // 50MHz xtal oscillator on the FPGA board (input to PLL)
logic core_clk;     // core clock (generated by the PLL)
logic int_clk;      // streamer clock (generated by the PLL)
logic streamer_clk; // streamer clock (int_clk or EXT_CLKp)

assign ref_clk = FPGA_CLK1_50;

// ref_clk (FPGA_CLK1_50) nominal frequency
localparam integer REF_CLK_FREQ_HZ = 50_000_000;

// Nominal streamer clock frequency (100MHz by default)
localparam integer STREAMER_CLK_FREQ_HZ = 100_000_000;

logic ext_clk_pll_locked;
logic clean_clk;
// Simple PLL instantiation for jitter attenuation
altpll my_pll (
  .inclk      ({1'b0, EXT_CLKp}), // inclk[0] is used, tie off [1]
  .areset     (reset),
  .clk        (clean_clk),     // regenerated clock
  .locked     (ext_clk_pll_locked)
);
defparam
  my_pll.bandwidth_type = "LOW",
  my_pll.inclk0_input_frequency = 100000, // input period in ps (10 MHz = 100,000 ps)
  my_pll.clk0_divide_by = 1,
  my_pll.clk0_multiply_by = 1,
  my_pll.clk0_phase_shift = "0",
  my_pll.operation_mode = "NORMAL",
  my_pll.compensate_clock = "CLK0";

wire [1:0] sel_clk; // used in SELECT_CLK case
wire clk_ena;

`ifdef INTERNAL_CLK
   assign streamer_clk = int_clk;
`elsif EXTERNAL_CLK
   assign streamer_clk = EXT_CLKp;
`elsif EXTERNAL_CLK_CLEAN
   assign streamer_clk = clean_clk;
`elsif SELECT_CLK
    altclkctrl #(
    .clock_type("GLOBAL CLOCK"),
    .ena_register_mode("none")
    ) u_clkctrl (
    .inclk     ({int_clk, '0, EXT_CLKp, '0}), // fitter may change order! test with "pptool -clk N"
    .clkselect (sel_clk),
    .ena       (clk_ena),
    .outclk    (streamer_clk)
    );
`elsif SELECT_CLK_CLEAN
   altclkctrl #(
    .clock_type("GLOBAL CLOCK"),
    .ena_register_mode("none")
    ) u_clkctrl (
    .inclk     ({int_clk, clean_clk, '0, '0}), // fitter may change order! test with "pptool -clk N"
    .clkselect (sel_clk),
    .ena       (clk_ena),
    .outclk    (streamer_clk)
    );
`else
    assign streamer_clk = 0;
`endif

// RESET LOGIC
// h2f_reset: general-purpose reset from HPS software or reset controllers; used as a
// functional reset line for FPGA subsystems.

logic h2f_reset, h2f_reset_n;
assign h2f_reset = ~h2f_reset_n;

// Reset logic (In reference clock domain)
logic [7:0] lock_cnt;
logic core_clk_pll_ready;
logic sys_reset_hold;

always_ff @(posedge ref_clk or posedge h2f_reset or negedge core_clk_pll_locked) begin
  if (h2f_reset | ~core_clk_pll_locked) begin
    core_clk_pll_ready <= 1'b0;
    sys_reset_hold <= 1'b1;
    lock_cnt <= 8'b0;
  end else begin
    if (core_clk_pll_locked) begin
      if (lock_cnt == 8'hFF) begin  // stable lock for 256 cycles
        core_clk_pll_ready <= 1'b1;
        sys_reset_hold <= 1'b0;
      end else begin
        lock_cnt <= lock_cnt + 1'b1;
      end
    end
  end
end

logic rst_out;
reset_sync2_hold #(
 .ACTIVE_LOW(0),
 .STAGES(3),
 .HOLD_CYCLES(16)
) reset_controller (
.clk(core_clk),
.rst1_in(h2f_reset),
.rst2_in(sys_reset_hold),
.rst_out(rst_out)
);

// inputs to reset controller
logic reset_in0, reset_in1;
assign reset_in0 = h2f_reset;
assign reset_in1 = sys_reset_hold;
// output from reset controller
logic reset_out;

// Reset signal to the FPGA design
logic reset;
//assign reset = rst_out;
assign reset = h2f_reset;

// Combined signals
logic [`WIDTH_DATA-1:0] streamer_qout;
logic streamer_qout_valid; // valid/enable signal

// Output from individual streamer cores
logic [`WIDTH_DATA-1:0] streamer1_qout;
logic streamer1_qout_valid; // valid/enable signal
logic [`WIDTH_DATA-1:0] streamer2_qout;
logic streamer2_qout_valid; // valid/enable signal
logic [`WIDTH_DATA-1:0] streamer3_qout;
logic streamer3_qout_valid; // valid/enable signal
logic [`WIDTH_DATA-1:0] streamer4_qout;
logic streamer4_qout_valid; // valid/enable signal

logic streamer1_qout_strobe,
      streamer1_strobe_enable,
      streamer1_done,
      streamer1_buffer_error,
      streamer1_trigger_armed,
      streamer1_trigger_activated;

logic streamer2_qout_strobe,
      streamer2_strobe_enable,
      streamer2_done,
      streamer2_buffer_error,
      streamer2_trigger_armed,
      streamer2_trigger_activated;

logic streamer3_qout_strobe,
      streamer3_strobe_enable,
      streamer3_done,
      streamer3_buffer_error,
      streamer3_trigger_armed,
      streamer3_trigger_activated;

logic streamer4_qout_strobe,
      streamer4_strobe_enable,
      streamer4_done,
      streamer4_buffer_error,
      streamer4_trigger_armed,
      streamer4_trigger_activated;

logic streamer_qout_strobe;
logic streamer_strobe_enable;
logic streamer_done;
logic streamer_buffer_error;

logic [`WIDTH_TRIGGER-1:0] streamer_trigger_in;                                // input
logic streamer_trigger_enable, streamer_trigger_reset, streamer_trigger_force; // input
logic streamer_trigger_armed, streamer_trigger_activated;                      // output

wire gate_in; // input

logic [`WIDTH_DATA-1:0] combiner_qout_in_in1;
logic [`WIDTH_DATA-1:0] combiner_qout_in_in2;
logic [`WIDTH_DATA-1:0] combiner_qout_in_in3;
logic [`WIDTH_DATA-1:0] combiner_qout_in_in4;
logic [`WIDTH_DATA-1:0] combiner_qout_out_o;

logic [`WIDTH_TRIGGER_ALL-1:0] combiner_trig_in_in1;
logic [`WIDTH_TRIGGER_ALL-1:0] combiner_trig_in_in2;
logic [`WIDTH_TRIGGER_ALL-1:0] combiner_trig_in_in3;
logic [`WIDTH_TRIGGER_ALL-1:0] combiner_trig_in_in4;
logic [`WIDTH_TRIGGER_ALL-1:0] combiner_trig_out_o;

logic [31:0] gp_in; // HPS-FPGA direct GPIO
logic [31:0] gp_out;

// Readback RLE encoder inputs
logic [`WIDTH_DATA-1:0] rl_qin;
logic rl_qin_valid;
logic rl_qin_strobe;
logic rl_qin_clk;

logic [`WIDTH_DATA-1:0] counter_q_input_data;
logic counter_q_input_clock;
logic counter_q_input_valid;

logic [3:0] freq_meter_0_conduit_end_signal;
logic freq_meter_0_conduit_end_clock;
logic freq_meter_0_conduit_end_reset;

logic ts_core_pps_sig;
logic ts_core_pps_sigA;

base_hps u0 (
      .clk_clk                           ( FPGA_CLK1_50 ), // 50MHz Xtal (input signal to PLLs)
      .reset_reset_n                     ( h2f_reset_n ),  // to FPGA core design
      .hps_0_h2f_reset_reset_n           ( h2f_reset_n ),  // from HPS

      .hps_0_ddr_mem_a                   ( HPS_DDR3_ADDR),
      .hps_0_ddr_mem_ba                  ( HPS_DDR3_BA),
      .hps_0_ddr_mem_ck                  ( HPS_DDR3_CK_P),
      .hps_0_ddr_mem_ck_n                ( HPS_DDR3_CK_N),
      .hps_0_ddr_mem_cke                 ( HPS_DDR3_CKE),
      .hps_0_ddr_mem_cs_n                ( HPS_DDR3_CS_N),
      .hps_0_ddr_mem_ras_n               ( HPS_DDR3_RAS_N),
      .hps_0_ddr_mem_cas_n               ( HPS_DDR3_CAS_N),
      .hps_0_ddr_mem_we_n                ( HPS_DDR3_WE_N),
      .hps_0_ddr_mem_reset_n             ( HPS_DDR3_RESET_N),
      .hps_0_ddr_mem_dq                  ( HPS_DDR3_DQ),
      .hps_0_ddr_mem_dqs                 ( HPS_DDR3_DQS_P),
      .hps_0_ddr_mem_dqs_n               ( HPS_DDR3_DQS_N),
      .hps_0_ddr_mem_odt                 ( HPS_DDR3_ODT),
      .hps_0_ddr_mem_dm                  ( HPS_DDR3_DM),
      .hps_0_ddr_oct_rzqin               ( HPS_DDR3_RZQ),

      .hps_0_io_hps_io_emac1_inst_TX_CLK ( HPS_ENET_GTX_CLK),
      .hps_0_io_hps_io_emac1_inst_TXD0   ( HPS_ENET_TX_DATA[0] ),
      .hps_0_io_hps_io_emac1_inst_TXD1   ( HPS_ENET_TX_DATA[1] ),
      .hps_0_io_hps_io_emac1_inst_TXD2   ( HPS_ENET_TX_DATA[2] ),
      .hps_0_io_hps_io_emac1_inst_TXD3   ( HPS_ENET_TX_DATA[3] ),
      .hps_0_io_hps_io_emac1_inst_RXD0   ( HPS_ENET_RX_DATA[0] ),
      .hps_0_io_hps_io_emac1_inst_MDIO   ( HPS_ENET_MDIO ),
      .hps_0_io_hps_io_emac1_inst_MDC    ( HPS_ENET_MDC  ),
      .hps_0_io_hps_io_emac1_inst_RX_CTL ( HPS_ENET_RX_DV),
      .hps_0_io_hps_io_emac1_inst_TX_CTL ( HPS_ENET_TX_EN),
      .hps_0_io_hps_io_emac1_inst_RX_CLK ( HPS_ENET_RX_CLK),
      .hps_0_io_hps_io_emac1_inst_RXD1   ( HPS_ENET_RX_DATA[1] ),
      .hps_0_io_hps_io_emac1_inst_RXD2   ( HPS_ENET_RX_DATA[2] ),
      .hps_0_io_hps_io_emac1_inst_RXD3   ( HPS_ENET_RX_DATA[3] ),

      .hps_0_io_hps_io_sdio_inst_CMD     ( HPS_SD_CMD         ),
      .hps_0_io_hps_io_sdio_inst_D0      ( HPS_SD_DATA[0]     ),
      .hps_0_io_hps_io_sdio_inst_D1      ( HPS_SD_DATA[1]     ),
      .hps_0_io_hps_io_sdio_inst_CLK     ( HPS_SD_CLK         ),
      .hps_0_io_hps_io_sdio_inst_D2      ( HPS_SD_DATA[2]     ),
      .hps_0_io_hps_io_sdio_inst_D3      ( HPS_SD_DATA[3]     ),

      .hps_0_io_hps_io_usb1_inst_D0      ( HPS_USB_DATA[0]    ),
      .hps_0_io_hps_io_usb1_inst_D1      ( HPS_USB_DATA[1]    ),
      .hps_0_io_hps_io_usb1_inst_D2      ( HPS_USB_DATA[2]    ),
      .hps_0_io_hps_io_usb1_inst_D3      ( HPS_USB_DATA[3]    ),
      .hps_0_io_hps_io_usb1_inst_D4      ( HPS_USB_DATA[4]    ),
      .hps_0_io_hps_io_usb1_inst_D5      ( HPS_USB_DATA[5]    ),
      .hps_0_io_hps_io_usb1_inst_D6      ( HPS_USB_DATA[6]    ),
      .hps_0_io_hps_io_usb1_inst_D7      ( HPS_USB_DATA[7]    ),
      .hps_0_io_hps_io_usb1_inst_CLK     ( HPS_USB_CLKOUT     ),
      .hps_0_io_hps_io_usb1_inst_STP     ( HPS_USB_STP        ),
      .hps_0_io_hps_io_usb1_inst_DIR     ( HPS_USB_DIR        ),
      .hps_0_io_hps_io_usb1_inst_NXT     ( HPS_USB_NXT        ),

      .hps_0_io_hps_io_uart0_inst_RX     ( HPS_UART_RX        ),
      .hps_0_io_hps_io_uart0_inst_TX     ( HPS_UART_TX        ),

      .hps_0_i2c1_clk_clk                (scl1_o_e),
      .hps_0_i2c1_scl_in_clk             (scl1_o),
      .hps_0_i2c1_out_data               (sda1_o_e),
      .hps_0_i2c1_sda                    (sda1_o),

      .hps_0_io_hps_io_i2c0_inst_SDA      (HPS_I2C1_SDAT),
      .hps_0_io_hps_io_i2c0_inst_SCL      (HPS_I2C1_SCLK),

      .hps_0_io_hps_io_gpio_inst_GPIO53  ( HPS_LED),
      .hps_0_io_hps_io_gpio_inst_GPIO54  ( HPS_KEY),

      .pb_pio_external_connection_export (KEY),
      .sw_pio_external_connection_export (SW),

      .pio_trig_int_external_connection_export     (pio_trig_int),     // output
      .pio_trig_monitor_external_connection_export (pio_trig_monitor), // input
      .pio_aux_external_connection_in_port         (pio_aux_in),       // input
      .pio_aux_external_connection_out_port        (pio_aux_out),      // output
      .pio_elapsed_external_connection_export      (pio_elapsed),      // input
      .pio_cfg_external_connection_export          (pio_cfg),          // output

      .pll_core_clk_locked_export(core_clk_pll_locked),
      .pll_int_clk_locked_export(int_clk_pll_locked),
      .pll_int_clk_outclk0_clk(int_clk),

      .clk_1_clk_clk(core_clk),

      .reset_controller_0_reset_in0_reset(reset_in0),
      .reset_controller_0_reset_in1_reset(reset_in1),
      .reset_controller_0_reset_out_reset(reset_out),

.st_interface_1_conduit_end_streamer_clk(streamer_clk),                     // input
.st_interface_1_conduit_end_qout(streamer1_qout),                           // output
.st_interface_1_conduit_end_qout_valid(streamer1_qout_valid),               // output
.st_interface_1_conduit_end_qout_strobe(streamer1_qout_strobe),             // output
.st_interface_1_conduit_end_strobe_enable(streamer1_strobe_enable),         // output
.st_interface_1_conduit_end_done(streamer1_done),                           // output
.st_interface_1_conduit_end_buffer_error(streamer1_buffer_error),           // output
.st_interface_1_conduit_end_trigger_in(streamer_trigger_in),                // input
.st_interface_1_conduit_end_trigger_enable(streamer_trigger_enable),        // input
.st_interface_1_conduit_end_trigger_reset(streamer_trigger_reset),          // input
.st_interface_1_conduit_end_trigger_force(streamer_trigger_force),          // input
.st_interface_1_conduit_end_trigger_armed(streamer1_trigger_armed),         // output
.st_interface_1_conduit_end_trigger_activated(streamer1_trigger_activated), // output
.st_interface_1_conduit_end_gate_in(gate_in),                               // input

.st_interface_2_conduit_end_streamer_clk(streamer_clk),                     // input
.st_interface_2_conduit_end_qout(streamer2_qout),                           // output
.st_interface_2_conduit_end_qout_valid(streamer2_qout_valid),               // output
.st_interface_2_conduit_end_qout_strobe(streamer2_qout_strobe),             // output
.st_interface_2_conduit_end_strobe_enable(streamer2_strobe_enable),         // output
.st_interface_2_conduit_end_done(streamer2_done),                           // output
.st_interface_2_conduit_end_buffer_error(streamer2_buffer_error),           // output
.st_interface_2_conduit_end_trigger_in(streamer_trigger_in),                // input
.st_interface_2_conduit_end_trigger_enable(streamer_trigger_enable),        // input
.st_interface_2_conduit_end_trigger_reset(streamer_trigger_reset),          // input
.st_interface_2_conduit_end_trigger_force(streamer_trigger_force),          // input
.st_interface_2_conduit_end_trigger_armed(streamer2_trigger_armed),         // output
.st_interface_2_conduit_end_trigger_activated(streamer2_trigger_activated), // output
.st_interface_2_conduit_end_gate_in(gate_in),                               // input

.st_interface_3_conduit_end_streamer_clk(streamer_clk),                     // input
.st_interface_3_conduit_end_qout(streamer3_qout),                           // output
.st_interface_3_conduit_end_qout_valid(streamer3_qout_valid),               // output
.st_interface_3_conduit_end_qout_strobe(streamer3_qout_strobe),             // output
.st_interface_3_conduit_end_strobe_enable(streamer3_strobe_enable),         // output
.st_interface_3_conduit_end_done(streamer3_done),                           // output
.st_interface_3_conduit_end_buffer_error(streamer3_buffer_error),           // output
.st_interface_3_conduit_end_trigger_in(streamer_trigger_in),                // input
.st_interface_3_conduit_end_trigger_enable(streamer_trigger_enable),        // input
.st_interface_3_conduit_end_trigger_reset(streamer_trigger_reset),          // input
.st_interface_3_conduit_end_trigger_force(streamer_trigger_force),          // input
.st_interface_3_conduit_end_trigger_armed(streamer3_trigger_armed),         // output
.st_interface_3_conduit_end_trigger_activated(streamer3_trigger_activated), // output
.st_interface_3_conduit_end_gate_in(gate_in),                               // input

.st_interface_4_conduit_end_streamer_clk(streamer_clk),                     // input
.st_interface_4_conduit_end_qout(streamer4_qout),                           // output
.st_interface_4_conduit_end_qout_valid(streamer4_qout_valid),               // output
.st_interface_4_conduit_end_qout_strobe(streamer4_qout_strobe),             // output
.st_interface_4_conduit_end_strobe_enable(streamer4_strobe_enable),         // output
.st_interface_4_conduit_end_done(streamer4_done),                           // output
.st_interface_4_conduit_end_buffer_error(streamer4_buffer_error),           // output
.st_interface_4_conduit_end_trigger_in(streamer_trigger_in),                // input
.st_interface_4_conduit_end_trigger_enable(streamer_trigger_enable),        // input
.st_interface_4_conduit_end_trigger_reset(streamer_trigger_reset),          // input
.st_interface_4_conduit_end_trigger_force(streamer_trigger_force),          // input
.st_interface_4_conduit_end_trigger_armed(streamer4_trigger_armed),         // output
.st_interface_4_conduit_end_trigger_activated(streamer4_trigger_activated), // output
.st_interface_4_conduit_end_gate_in(gate_in),                               // input

.rl_encoder_if_conduit_end_qin        (rl_qin),
.rl_encoder_if_conduit_end_qin_valid  (rl_qin_valid),
.rl_encoder_if_conduit_end_qin_strobe (rl_qin_strobe),
.rl_encoder_if_conduit_end_qin_clk    (rl_qin_clk),

.combiner_qout_in_in1,
.combiner_qout_in_in2,
.combiner_qout_in_in3,
.combiner_qout_in_in4,
.combiner_qout_out_o,
.combiner_qout_clk_clk(streamer_clk),

.combiner_trig_in_in1,
.combiner_trig_in_in2,
.combiner_trig_in_in3,
.combiner_trig_in_in4,
.combiner_trig_out_o,
.combiner_trig_clk_clk(streamer_clk),

.counter_q_input_data(counter_q_input_data),
.counter_q_input_clock(counter_q_input_clock),
.counter_q_input_valid(counter_q_input_valid),

.freq_meter_0_conduit_end_signal(freq_meter_0_conduit_end_signal),
.freq_meter_0_conduit_end_clock(freq_meter_0_conduit_end_clock),
.freq_meter_0_conduit_end_reset(freq_meter_0_conduit_end_reset),

.ts_core_pps_conduit_end_sig(ts_core_pps_sig),
.ts_core_pps_conduit_end_sigA(ts_core_pps_sigA),

// 32-Bit direct access registry between HPS and FPGA
.hps_0_h2f_gp_gp_in    (gp_in),    // FPGA to HPS -->
.hps_0_h2f_gp_gp_out   (gp_out)    // HPS to FPGA <--
);

assign gp_in[0] = core_clk_pll_locked;
assign gp_in[1] = int_clk_pll_locked;
`ifdef EXTERNAL_CLK_CLEAN
   assign gp_in[2] = ext_clk_pll_locked;
`else
   assign gp_in[2] = 0;
`endif
assign gp_in[3] = core_clk_pll_ready;
assign gp_in[4] = sys_reset_hold;
assign gp_in[5] = activity;
assign gp_in[6] = rst_out;
assign gp_in[7] = reset_out;
assign gp_in[31:8] = 0;

// Clock source switching
assign sel_clk = gp_out[1:0]; // bits [0:1] control clock source
assign clk_ena = 'b1;
//assign clk_ena = gp_out[2];   // note: on power-up, clock is disabled

logic oe; // output enable
assign oe = pio_cfg[`CFG_OE];

logic heartbeat;
heartbeat #(
 .CLK_FREQ_HZ(STREAMER_CLK_FREQ_HZ),
 .PULSE_MS(80),
 .GAP_MS(70),
 .PERIOD_MS(1000)
) hb_inst (
 .clk(core_clk),
 .reset(reset),
 .heartbeat(heartbeat)
);

logic streamer_rst; // reset synchronized to the stream_clk clock domain
sync_bit_3stage sb(
 .clk_dest(streamer_clk),
 .async_in(reset),
 .sync_out(streamer_rst)
);

logic activity;
presence_detector_async_posedge #(
 .CLK_FREQ_HZ(STREAMER_CLK_FREQ_HZ),
 .WINDOW_MS(200)    // active if signal has at least one posedge within 200 ms
) u_activity_monitor (
 .clk(streamer_clk),
 .reset(streamer_rst),
 .sig_in(streamer_qout_strobe),
 .active(activity)
);

assign freq_meter_0_conduit_end_signal[0] = clean_clk;    // external clock after PLL cleaning
assign freq_meter_0_conduit_end_signal[1] = int_clk;      // internal clock generate by the PLL
assign freq_meter_0_conduit_end_signal[2] = streamer_clk; // currently selected streamer clock
assign freq_meter_0_conduit_end_signal[3] = core_clk;     // PulsePins core clock
assign freq_meter_0_conduit_end_clock = ref_clk;          // 50MHz xtal clock
assign freq_meter_0_conduit_end_reset = reset;            // reset in the ref_clk clock domain

// Onboard LEDs on DE10-Nano
// Recall: LEDs are not inverted, they show signals as they are.
// LED[0] is the one closest to the Ethernet port.
assign LED[0] = streamer_trigger_armed;
assign LED[1] = streamer_trigger_activated;
assign LED[2] = streamer_done;
assign LED[3] = streamer_buffer_error;
assign LED[4] = streamer_trigger_in[0];
assign LED[5] = streamer_trigger_in[1];
assign LED[6] = activity;
assign LED[7] = heartbeat;

// Output
// qout[0] - D4 (yellow)
// qout[1] - D5 (green)
// qout[2] - D6 (blue)
// qout[3] - D7 (violet)

`define USE_IOBUF
`ifdef USE_IOBUF
   logic [`WIDTH_DATA-1:0] q_in;
   logic q_in_valid;
   logic q_in_strobe;
   genvar j;
   generate
     for (j = 0; j < `WIDTH_DATA; j = j + 1) begin : gen_iobuf
       ALT_IOBUF iobuf_inst (
         .i(streamer_qout[j]),
         .oe(oe),
         .o(q_in[j]),
         .io(GPI1Q[j])
       );
     end
   endgenerate
   ALT_IOBUF valid_buf_inst (
     .i(streamer_qout_valid),
     .oe(oe),
     .o(q_in_valid),
     .io(GPI0GPIO3)
   );
   ALT_IOBUF strobe_buf_inst (
     .i(streamer_qout_strobe),
     .oe(oe),
     .o(q_in_strobe),
     .io(GPI0GPIO0)
   );
   assign rl_qin        = (oe ? streamer_qout        : q_in);
   assign rl_qin_valid  = (oe ? streamer_qout_valid  : q_in_valid);
   assign rl_qin_strobe = (oe ? streamer_qout_strobe : q_in_strobe);
   assign rl_qin_clk    = streamer_clk;
   assign counter_q_input_data =  (oe ? streamer_qout       : q_in);
   assign counter_q_input_clock = streamer_clk;
   assign counter_q_input_valid = (oe ? streamer_qout_valid : q_in_valid);
`else
   assign GPI1Q        = streamer_qout;
   assign GPI0GPIO3    = streamer_qout_valid;
   assign GPI0GPIO0    = streamer_qout_strobe;
   assign rl_qin         = streamer_qout;
   assign rl_qin_valid   = streamer_qout_valid;
   assign rl_qin_strobe  = streamer_qout_strobe;
   assign rl_qin_clk     = streamer_clk;
   assign counter_q_input_data  = streamer_qout;
   assign counter_q_input_clock = streamer_clk;
   assign counter_q_input_valid = streamer_qout_valid;
`endif

// clocking (orange)
// GPI0GPIO[0] <-> streamer_qout_strobe;             // data strobe, D0 (black)
assign GPI0GPIO1 = oe;                               // output enable, D1 (brown)
assign GPI0GPIO2 = streamer_clk;                     // streamer clock, D2 (red)
// GPI0GPIO[3] <-> streamer_qout_valid;              // valid/enable signal, D3 (orange)

// monitoring
assign GPI0GPIO4 = activity;                         // activity (on when data is streamed out)
assign GPI0GPIO5 = heartbeat;                        // pulses when FPGA is programmed

// streaming and trigger status [OUTPUTS] (green)
assign GPI0GPIO6 = streamer_trigger_armed;           // D8~d0 (black)
assign GPI0GPIO7 = streamer_trigger_activated;       // D9~d1 (brown)
assign GPI0GPIO8 = streamer_done;                    // D10~d2 (red)
assign GPI0GPIO9 = streamer_buffer_error;            // D11~d3 (buffer_error)

// trigger control [INPUTS]
logic ext_trigger_enable, ext_trigger_force, ext_trigger_reset;
assign ext_trigger_enable = GPI0GPIO10;
assign ext_trigger_force  = GPI0GPIO11;
assign ext_trigger_reset  = GPI0GPIO12;

// gating
assign gate_in            = GPI0GPIO13;

// trigger signals [INPUTS]
logic [`WIDTH_TRIGGER-1:0] ext_trigger_in;
assign ext_trigger_in = GPI0TRIG;

// GPIO0GPIO[25:22] are extra signals that may be configures in various ways.
// Default is setA.
//`define EXTRA_SETA
`define EXTRA_SETB

`ifdef EXTRA_SETA
// trigger control and signals as seen by the streamer core [OUTPUT]
assign GPI0GPIO22 = streamer_trigger_enable;         // D12~d4 (yellow)
assign GPI0GPIO23 = streamer_trigger_force;          // D13~d5 (green)
assign GPI0GPIO24 = streamer_trigger_reset;          // D14~d6 (blue)
assign GPI0GPIO25 = streamer_trigger_in[0];          // D15~d7 (violet)
`endif

`ifdef EXTRA_SETB
wire rnd1, rnd2, rnd3, rnd4;
assign GPI0GPIO22 = rnd1;
assign GPI0GPIO23 = rnd2;
assign GPI0GPIO24 = rnd3;
assign GPI0GPIO25 = rnd4;

rand_signal_gen #(
  .MIN_PERIOD_CYCLES (1),
  .MAX_PERIOD_CYCLES (100),
  .SEED              (32'h1234_5678)
) rand1 (
  .clk    (ref_clk), // 50MHz, period = 20ns
  .reset  (reset),
  .oe     ('1),
  .signal (rnd1)
);

rand_signal_gen #(
  .MIN_PERIOD_CYCLES (100),
  .MAX_PERIOD_CYCLES (10_000_000),
  .SEED              (32'h1234_5678)
) rand2 (
  .clk    (ref_clk),
  .reset  (reset),
  .oe     ('1),
  .signal (rnd2)
);

`endif

// I2C
ALT_IOBUF i2c1_scl_iobuf   (
 .i(1'b0),      // always drive '0' when enabled (open-drain)
 .oe(scl1_o_e), // from HPS: 1 = pull low, 0 = release
 .o(scl1_o),    // to HPS: sampled bus level
 .io(GPI0GPIO27)
);
ALT_IOBUF i2c1_sda_iobuf   (
 .i(1'b0),
 .oe(sda1_o_e),
 .o(sda1_o),
 .io(GPI0GPIO26)
);

// Auxiliary i/o
logic [`WIDTH_AUX-1:0] aux_oe;                                                 // i/o direction select
assign aux_oe = pio_cfg[`CFG_AUX_DIR_OFFSET+`WIDTH_AUX-1:`CFG_AUX_DIR_OFFSET]; // 1 = out, 0 = in (default: input)
genvar k;
generate
  // NOTE: when aux_oe=1, i drives the pin and o reflects the voltage on the pad (in practice the driven value,
  // unless there is bus contention)
  for (k = 0; k < `WIDTH_AUX; k = k + 1) begin : gen_iobuf_aux
    ALT_IOBUF iobuf_aux_inst (
      .i(pio_aux_out[k]),
      .oe(aux_oe[k]),
      .o(pio_aux_in[k]),
      .io(GPI0AUX[k])
    );
  end
endgenerate

// internal trigger signals and control (via pio_trig_int GPIO)
logic [`WIDTH_TRIGGER-1:0] int_trigger_in;
logic int_trigger_enable, int_trigger_force, int_trigger_reset;
assign int_trigger_in     = pio_trig_int[`WIDTH_TRIGGER-1:0];
assign int_trigger_enable = pio_trig_int[`POS_TRIG_ENABLE];
assign int_trigger_force  = pio_trig_int[`POS_TRIG_FORCE];
assign int_trigger_reset  = pio_trig_int[`POS_TRIG_RESET];

// push-button trigger signals
logic [1:0] pb_trigger_in;
assign pb_trigger_in[0] = ~KEY[0]; // right button (further away from the GPIO connector, next to Ethernet port)
assign pb_trigger_in[1] = ~KEY[1]; // left button (closer to the GPIO connector)

// switch trigger control
// SW[0] is right-most (closest to the Ethernet port)
// SW is ON when it is 'up' (closer to the FPGA chip)
logic sw_trigger_enable, sw_trigger_force, sw_trigger_reset;
assign sw_trigger_enable = SW[0];
assign sw_trigger_force  = SW[1];
assign sw_trigger_reset  = SW[2];

`define TRIGGER_COMBINER
`ifdef TRIGGER_COMBINER
   assign combiner_trig_in_in1[`WIDTH_TRIGGER-1:0] = int_trigger_in[`WIDTH_TRIGGER-1:0];
   assign combiner_trig_in_in1[`POS_TRIG_ENABLE]   = int_trigger_enable;
   assign combiner_trig_in_in1[`POS_TRIG_FORCE]    = int_trigger_force;
   assign combiner_trig_in_in1[`POS_TRIG_RESET]    = int_trigger_reset;

   assign combiner_trig_in_in2[`WIDTH_TRIGGER-1:0] = ext_trigger_in[`WIDTH_TRIGGER-1:0];
   assign combiner_trig_in_in2[`POS_TRIG_ENABLE]   = ext_trigger_enable;
   assign combiner_trig_in_in2[`POS_TRIG_FORCE]    = ext_trigger_force;
   assign combiner_trig_in_in2[`POS_TRIG_RESET]    = ext_trigger_reset;

   assign combiner_trig_in_in3[1:0]                = pb_trigger_in[1:0];
   assign combiner_trig_in_in3[2]                  = PPS_IN;
   assign combiner_trig_in_in3[`WIDTH_TRIGGER-1:3] = '0;
   assign combiner_trig_in_in3[`POS_TRIG_ENABLE]   = sw_trigger_enable;
   assign combiner_trig_in_in3[`POS_TRIG_FORCE]    = sw_trigger_force;
   assign combiner_trig_in_in3[`POS_TRIG_RESET]    = sw_trigger_reset;

   `define TRIGGER_AUX
   `ifdef TRIGGER_AUX
      assign combiner_trig_in_in4[`WIDTH_TRIGGER-1:0] = pio_aux_in;
   `else
      assign combiner_trig_in_in4[`WIDTH_TRIGGER-1:0] = '0;
   `endif

   assign combiner_trig_in_in4[`POS_TRIG_ENABLE]   = '0;
   assign combiner_trig_in_in4[`POS_TRIG_FORCE]    = '0;
   assign combiner_trig_in_in4[`POS_TRIG_RESET]    = '0;

   assign streamer_trigger_in[`WIDTH_TRIGGER-1:0] = combiner_trig_out_o[`WIDTH_TRIGGER-1:0];
   assign streamer_trigger_enable  = combiner_trig_out_o[`POS_TRIG_ENABLE];
   assign streamer_trigger_force   = combiner_trig_out_o[`POS_TRIG_FORCE];
   assign streamer_trigger_reset   = combiner_trig_out_o[`POS_TRIG_RESET];
`else
   assign streamer_trigger_enable  = int_trigger_enable;
   assign streamer_trigger_force   = int_trigger_force;
   assign streamer_trigger_reset   = int_trigger_reset;
   assign streamer_trigger_in[`WIDTH_TRIGGER-1:0] = int_trigger_in[`WIDTH_TRIGGER-1:0];
`endif

assign combiner_qout_in_in1 = streamer1_qout;
assign combiner_qout_in_in2 = streamer2_qout;
assign combiner_qout_in_in3 = streamer3_qout;
assign combiner_qout_in_in4 = streamer4_qout;

`define QOUT_COMBINER
`ifdef QOUT_COMBINER
   delay_or4 u_qout_valid        (.clk(streamer_clk), .in1(streamer1_qout_valid), .in2(streamer2_qout_valid),
                                  .in3(streamer3_qout_valid), .in4(streamer4_qout_valid), .dout(streamer_qout_valid));
   delay_or4 u_strobe_enable     (.clk(streamer_clk), .in1(streamer1_strobe_enable), .in2(streamer2_strobe_enable),
                                  .in3(streamer3_strobe_enable), .in4(streamer4_strobe_enable), .dout(streamer_strobe_enable));
   delay_or4 u_done              (.clk(streamer_clk), .in1(streamer1_done), .in2(streamer2_done),
                                  .in3(streamer3_done), .in4(streamer4_done), .dout(streamer_done));
   delay_or4 u_buffer_error      (.clk(streamer_clk), .in1(streamer1_buffer_error), .in2(streamer2_buffer_error),
                                  .in3(streamer3_buffer_error), .in4(streamer4_buffer_error), .dout(streamer_buffer_error));
   delay_or4 u_trigger_armed     (.clk(streamer_clk), .in1(streamer1_trigger_armed), .in2(streamer2_trigger_armed),
                                  .in3(streamer3_trigger_armed), .in4(streamer4_trigger_armed), .dout(streamer_trigger_armed));
   delay_or4 u_trigger_activated (.clk(streamer_clk), .in1(streamer1_trigger_activated), .in2(streamer2_trigger_activated),
                                   .in3(streamer3_trigger_activated), .in4(streamer4_trigger_activated),
                                  .dout(streamer_trigger_activated));
   assign streamer_qout_strobe = streamer_qout_valid && ~streamer_clk; // synthesized, not obtained from streamer signals!
   assign streamer_qout        = combiner_qout_out_o;
`else
   // Use streamer 0 (#1)
   assign streamer_qout              = streamer1_qout;
   assign streamer_qout_valid        = streamer1_qout_valid;
   assign streamer_qout_strobe       = streamer1_qout_strobe;
   assign streamer_strobe_enable     = streamer1_strobe_enable;
   assign streamer_done              = streamer1_done;
   assign streamer_buffer_error      = streamer1_buffer_error;
   assign streamer_trigger_armed     = streamer1_trigger_armed;
   assign streamer_trigger_activated = streamer1_trigger_activated;
`endif

// Monitoring of trigger signals via pio_trig_monitor GPIO

assign pio_trig_monitor[`WIDTH_TRIGGER-1:0]                   = ext_trigger_in[`WIDTH_TRIGGER-1:0]; // external trigger signals on the FPGA pins
assign pio_trig_monitor[`POS_TRIG_ENABLE]                     = ext_trigger_enable;
assign pio_trig_monitor[`POS_TRIG_FORCE]                      = ext_trigger_force;
assign pio_trig_monitor[`POS_TRIG_RESET]                      = ext_trigger_reset;
assign pio_trig_monitor[15:11]                                = '0;

`define PIO_TRIG_OFFSET 16
assign pio_trig_monitor[`PIO_TRIG_OFFSET+`WIDTH_TRIGGER-1:16] = streamer_trigger_in; // trigger signals as seen by the streamer
assign pio_trig_monitor[`PIO_TRIG_OFFSET+`POS_TRIG_ENABLE]    = streamer_trigger_enable;
assign pio_trig_monitor[`PIO_TRIG_OFFSET+`POS_TRIG_FORCE]     = streamer_trigger_enable;
assign pio_trig_monitor[`PIO_TRIG_OFFSET+`POS_TRIG_RESET]     = streamer_trigger_enable;
assign pio_trig_monitor[31:27]                                = '0;

logic PPS_XTAL; // not a true atomic-clock-derived PPS, just a divided crystal oscillator clock
tik #(.PERIOD(REF_CLK_FREQ_HZ)) tikpps (
 .clk(ref_clk),
 .reset(reset),
 .tik(PPS_XTAL)
);

// Counts elapsed time (in seconds, according to PPS_XTAL) since last reset
logic [31:0] elapsed;
always @(posedge reset or posedge PPS_XTAL) begin
  if (reset) begin
    elapsed <= '0;
  end else begin
    elapsed <= elapsed + 1;
  end
end
assign pio_elapsed = elapsed;

// Arduino port (replicated qout signals + clocks) for debugging purposes
`ifdef ARDUINO_DEBUG_PORT
   assign ARDUINO_IO[7:0]   = streamer_qout[7:0];
   assign ARDUINO_IO[8]     = streamer_clk;
   assign ARDUINO_IO[9]     = core_clk;
   assign ARDUINO_IO[10]    = int_clk;
   assign ARDUINO_IO[12:11] = streamer_qout[1:0];
   assign ARDUINO_IO[13]    = PPS_XTAL;
   assign ARDUINO_IO[15:14] = '0;
`endif

// Pulse generation (synchronous with streamer_clk)
logic pulse_1ms;
logic pulse_10ms;
logic pulse_100ms;
logic pulse_1s;

pulse_gen_timebase #( .CLK_FREQ_HZ(STREAMER_CLK_FREQ_HZ) ) pg_inst (
 .clk(streamer_clk),
 .rst(reset),
 .pulse_1ms(pulse_1ms),     // 1 s pulse (every 1 ms)
 .pulse_10ms(pulse_10ms),   // 0.01 s pulse (every 10 ms)
 .pulse_100ms(pulse_100ms), // 0.1 s pulse (every 100 ms)
 .pulse_1s(pulse_1s)        // 1 s pulse (every 1000 ms)
);

// Time-stamping logic
logic ts_sel_pps; // Select PPS input signal: 0 = PPS_XTAL, 1 = PPS_IN
assign ts_sel_pps = pio_cfg[`CFG_TS_SEL_PPS];
assign ts_core_pps_sig = (ts_sel_pps ? PPS_IN : PPS_XTAL);

`define TS_SIGA_MUX
`ifdef TS_SIGA_MUX
  logic [TS_SIGA_MUX_WIDTH-1:0] ts_sel_sigA;
  assign ts_sel_sigA = pio_cfg[`CFG_TS_SEL_SIGA_MUX_OFFSET+TS_SIGA_MUX_WIDTH-1:`CFG_TS_SEL_SIGA_MUX_OFFSET];
  sig_mux #( .INPUTS(TS_SIGA_MUX_NR_IN) ) ts_mux_inst (
   .sel(ts_sel_sigA),
   .i( { pulse_1ms,
         pulse_10ms,
         pulse_100ms,
         pulse_1s,
         pio_aux_in[0],
         ext_trigger_in[0],
         streamer_trigger_in[0],
         streamer_trigger_activated } ),
   .o(ts_core_pps_sigA)
  );
`else
  assign ts_core_pps_sigA = streamer_trigger_activated;
`endif

endmodule : pulsepins

`default_nettype wire
