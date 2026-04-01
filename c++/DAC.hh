// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Helpers for controlling AD5693-family DACs over I2C.

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "I2C.hh"

// AD5693/AD5693R command bytes (upper nibble is command; low nibble don't care)
static constexpr uint8_t CMD_WRITE_DAC_AND_INPUT = 0x30; // updates output
static constexpr uint8_t CMD_WRITE_CONTROL       = 0x40; // control register

class AD5693 {
public:
  AD5693(I2CDevice& i2c, uint8_t addr7) : i2c_(i2c), addr7_(addr7) {
    if (addr7_ < 0x03 || addr7_ > 0x77) {
      throw std::invalid_argument("I2C address out of 7-bit range");
    }
  }

  void write_control(bool gain_2x, bool ref_disable) {
    // Control register bits (D15..D11): RESET, PD1, PD0, REF, GAIN.
    // Keep RESET=0, PD=0 (normal mode). Set REF/GAIN as requested.
    uint16_t control = 0;
    if (ref_disable) control |= (1u << 12); // REF bit
    if (gain_2x)     control |= (1u << 11); // GAIN bit
    write_cmd16(CMD_WRITE_CONTROL, control);
  }

  void write_code(uint16_t code) {  // raw 16-bit DAC code
    write_cmd16(CMD_WRITE_DAC_AND_INPUT, code);
  }

  // Convenience: set by voltage. Returns the 16-bit code that was sent.
  uint16_t set_voltage(double vout, double vref, int gain) {
    if (!(vref > 0.0)) throw std::invalid_argument("vref must be > 0");
    if (!(gain == 1 || gain == 2)) throw std::invalid_argument("gain must be 1 or 2");
    const double full_scale = vref * static_cast<double>(gain);
    constexpr uint32_t max_code = 0xFFFFu;
    const double code_f = (vout / full_scale) * static_cast<double>(max_code);
    long long code_ll = llround(code_f);
    if (code_ll < 0) code_ll = 0;
    if (code_ll > static_cast<long long>(max_code)) code_ll = static_cast<long long>(max_code);
    const uint16_t code = static_cast<uint16_t>(code_ll);
    if (verbose) std::cout << "code=0x" << std::hex << code << std::endl;
    write_code(code);
    return code;
  }

private:
  void write_cmd16(uint8_t cmd, uint16_t word) {
    const uint8_t hi = static_cast<uint8_t>((word >> 8) & 0xFF);
    const uint8_t lo = static_cast<uint8_t>(word & 0xFF);
    i2c_.write_reg2(addr7_, cmd, {hi, lo});
  }

  I2CDevice& i2c_;
  uint8_t addr7_;
  bool verbose = false;
};
