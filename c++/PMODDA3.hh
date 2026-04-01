// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <stdexcept>

#include "SPI.hh"

namespace pmod_da3 {

inline spi::Config default_spi_config()
{
  spi::Config cfg;
  cfg.decoder_clock_hz = 100e6;
  cfg.spi_clock_hz = 10e6;
  cfg.mode = 0;
  cfg.bit_order = spi::BitOrder::MSB_FIRST;
  cfg.bit_sclk = 3;
  cfg.bit_mosi = 1;
  cfg.bit_cs = 0;
  cfg.bit_aux = 2; // ~LDAC, held low for immediate updates
  cfg.chip_select_active_high = false;
  cfg.aux_default = false;
  cfg.select_setup_ticks = 2;
  cfg.deselect_hold_ticks = 2;
  cfg.post_idle_ticks = 4;
  return cfg;
}

inline uint16_t code_from_voltage(double volts, double vref = 2.5)
{
  if (vref <= 0.0)
    throw std::invalid_argument("vref must be positive");

  const double clipped = std::max(0.0, std::min(volts, vref));
  const double code = std::round((65536.0 * clipped) / vref);
  if (code <= 0.0)
    return 0x0000;
  if (code >= 65535.0)
    return 0xFFFF;
  return static_cast<uint16_t>(code);
}

inline spi::SequenceBuilder transaction_for_code(uint16_t code,
                                                 spi::Config cfg = default_spi_config())
{
  spi::SequenceBuilder builder(cfg);
  builder.set_aux(false);
  builder.write_word16_transaction(code);
  return builder;
}

inline spi::SequenceBuilder transaction_for_voltage(double volts,
                                                    const double vref = 2.5,
                                                    spi::Config cfg = default_spi_config())
{
  return transaction_for_code(code_from_voltage(volts, vref), cfg);
}

} // namespace pmod_da3
