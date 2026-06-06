// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <cstdint>

#include "hps_0.h"

// `hps_0.h` merges regions from multiple bus masters whose numeric offsets can overlap.
// Keep the master explicit at project-specific constructor boundaries.
namespace address_map {

struct H2fRegion {
  std::uintptr_t base;
  std::uintptr_t span;
};

struct LwRegion {
  std::uintptr_t base;
  std::uintptr_t span;
};

constexpr bool contains(H2fRegion region, std::uintptr_t offset, std::uintptr_t bytes = sizeof(std::uint32_t)) noexcept {
  return offset <= region.span && bytes <= region.span - offset;
}

constexpr bool contains(LwRegion region, std::uintptr_t offset, std::uintptr_t bytes = sizeof(std::uint32_t)) noexcept {
  return offset <= region.span && bytes <= region.span - offset;
}

namespace h2f {

inline constexpr H2fRegion rl_encoder_if {RL_ENCODER_IF_BASE, RL_ENCODER_IF_SPAN};
inline constexpr H2fRegion fifo_ts_pps_out {FIFO_TS_PPS_OUT_BASE, FIFO_TS_PPS_OUT_SPAN};
inline constexpr H2fRegion fifo_ts_pps_in_csr {FIFO_TS_PPS_IN_CSR_BASE, FIFO_TS_PPS_IN_CSR_SPAN};
inline constexpr H2fRegion fifo_ts_siga_out {FIFO_TS_SIGA_OUT_BASE, FIFO_TS_SIGA_OUT_SPAN};
inline constexpr H2fRegion fifo_ts_siga_in_csr {FIFO_TS_SIGA_IN_CSR_BASE, FIFO_TS_SIGA_IN_CSR_SPAN};
inline constexpr H2fRegion msgdma_1_csr {MSGDMA_1_CSR_BASE, MSGDMA_1_CSR_SPAN};
inline constexpr H2fRegion st_mux_1 {ST_MUX_1_BASE, ST_MUX_1_SPAN};
inline constexpr H2fRegion msgdma_1_descriptor_slave {MSGDMA_1_DESCRIPTOR_SLAVE_BASE, MSGDMA_1_DESCRIPTOR_SLAVE_SPAN};
inline constexpr H2fRegion fifo_1_in {FIFO_1_IN_BASE, FIFO_1_IN_SPAN};
inline constexpr H2fRegion fifo_2_in {FIFO_2_IN_BASE, FIFO_2_IN_SPAN};
inline constexpr H2fRegion fifo_3_in {FIFO_3_IN_BASE, FIFO_3_IN_SPAN};
inline constexpr H2fRegion fifo_4_in {FIFO_4_IN_BASE, FIFO_4_IN_SPAN};
inline constexpr H2fRegion fifo_1_in_csr {FIFO_1_IN_CSR_BASE, FIFO_1_IN_CSR_SPAN};
inline constexpr H2fRegion fifo_2_in_csr {FIFO_2_IN_CSR_BASE, FIFO_2_IN_CSR_SPAN};
inline constexpr H2fRegion fifo_3_in_csr {FIFO_3_IN_CSR_BASE, FIFO_3_IN_CSR_SPAN};
inline constexpr H2fRegion fifo_4_in_csr {FIFO_4_IN_CSR_BASE, FIFO_4_IN_CSR_SPAN};
inline constexpr H2fRegion st_interface_1 {ST_INTERFACE_1_BASE, ST_INTERFACE_1_SPAN};
inline constexpr H2fRegion st_interface_2 {ST_INTERFACE_2_BASE, ST_INTERFACE_2_SPAN};
inline constexpr H2fRegion st_interface_3 {ST_INTERFACE_3_BASE, ST_INTERFACE_3_SPAN};
inline constexpr H2fRegion st_interface_4 {ST_INTERFACE_4_BASE, ST_INTERFACE_4_SPAN};
inline constexpr H2fRegion sysid_h2f {SYSID_H2F_BASE, SYSID_H2F_SPAN};
inline constexpr H2fRegion combiner_trig {COMBINER_TRIG_BASE, COMBINER_TRIG_SPAN};
inline constexpr H2fRegion combiner_qout {COMBINER_QOUT_BASE, COMBINER_QOUT_SPAN};
inline constexpr H2fRegion fifo_rl_out {FIFO_RL_OUT_BASE, FIFO_RL_OUT_SPAN};
inline constexpr H2fRegion fifo_rl_in_csr {FIFO_RL_IN_CSR_BASE, FIFO_RL_IN_CSR_SPAN};
inline constexpr H2fRegion counter_q {COUNTER_Q_BASE, COUNTER_Q_SPAN};
inline constexpr H2fRegion freq_meter_0 {FREQ_METER_0_BASE, FREQ_METER_0_SPAN};

}

namespace lw {

inline constexpr LwRegion sysid {SYSID_BASE, SYSID_SPAN};
inline constexpr LwRegion sysid_qsys_0 {SYSID_QSYS_0_BASE, SYSID_QSYS_0_SPAN};
inline constexpr LwRegion sysid_qsys_1 {SYSID_QSYS_1_BASE, SYSID_QSYS_1_SPAN};
inline constexpr LwRegion pio_trig_int {PIO_TRIG_INT_BASE, PIO_TRIG_INT_SPAN};
inline constexpr LwRegion pio_trig_monitor {PIO_TRIG_MONITOR_BASE, PIO_TRIG_MONITOR_SPAN};
inline constexpr LwRegion pio_aux {PIO_AUX_BASE, PIO_AUX_SPAN};
inline constexpr LwRegion pio_cfg {PIO_CFG_BASE, PIO_CFG_SPAN};
inline constexpr LwRegion pio_elapsed {PIO_ELAPSED_BASE, PIO_ELAPSED_SPAN};
inline constexpr LwRegion pll_reconfig_int_clk {PLL_RECONFIG_INT_CLK_BASE, PLL_RECONFIG_INT_CLK_SPAN};
inline constexpr LwRegion pll_reconfig_core_clk {PLL_RECONFIG_CORE_CLK_BASE, PLL_RECONFIG_CORE_CLK_SPAN};

}

}
