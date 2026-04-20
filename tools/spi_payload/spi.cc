#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "spi_rle_encoder.hh"

using spi_payload::SpiRleEncoder;

static uint16_t voltage_to_code(double volts, double vref = 2.5) {
  if (vref <= 0.0)
    throw std::invalid_argument("vref must be positive");
  const double clipped = std::clamp(volts, 0.0, vref);
  const double code = std::round((65536.0 * clipped) / vref);
  if (code <= 0.0)
    return 0x0000;
  if (code >= 65535.0)
    return 0xFFFF;
  return static_cast<uint16_t>(code);
}

int main() {
  SpiRleEncoder::Config cfg;
  cfg.decoder_clock_hz = 100e6;
  cfg.spi_clock_hz = 10e6;
  cfg.mode = 0;
  cfg.bit_order = SpiRleEncoder::BitOrder::MSB_FIRST;
  // Common SPI convention: select is active low on the wire
  cfg.chip_select_active_high = false;
  cfg.select_setup_ticks = 2;
  cfg.deselect_hold_ticks = 2;
  cfg.post_idle_ticks = 4;
  cfg.aux_default = false; // AUX = LDAC = low permanently

  uint16_t code = voltage_to_code(2.5);
  std::cout << "code=" << std::dec << code << " 0x" << std::hex << code << std::endl;

  SpiRleEncoder enc(cfg);
  enc.set_aux(false);        // explicit, though aux_default=false already does this
  enc.select();
  enc.write_word16(code);
  enc.deselect();

  std::cout << "Requested SPI clock: " << cfg.spi_clock_hz << " Hz\n";
  std::cout << "Achieved  SPI clock: " << enc.achieved_spi_clock_hz() << " Hz\n";
  std::cout << "Half-period ticks:   " << enc.half_period_ticks() << "\n\n";

  dump_tokens(enc.tokens());

  std::cout << "SCLK ~LDAC DIN ~CS" << std::endl; // pin 3 to 0

  std::ofstream F("sequence");
  spi_payload::dump_tokens_seq(enc.tokens(), F);
  return 0;
}
