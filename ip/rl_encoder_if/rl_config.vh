// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`ifndef RL_CONFIG_VH
`define RL_CONFIG_VH

package cfg;
 parameter int WIDTH_AVS = 32;
 parameter int WIDTH_DATA = 32;
 parameter int WIDTH_COUNTER = 32;
 parameter int WIDTH_CONTROL = 32;
 parameter int WIDTH_TOTAL = WIDTH_DATA+WIDTH_COUNTER+WIDTH_CONTROL;
endpackage

`endif
