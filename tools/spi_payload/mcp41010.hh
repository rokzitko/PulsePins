#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "spi_rle_encoder.hh"

namespace spi_payload {

class Mcp41010 {
 public:
  enum class Command { Write, Shutdown };

  static constexpr uint8_t kMinWiper = 0x00;
  static constexpr uint8_t kMidWiper = 0x80;
  static constexpr uint8_t kMaxWiper = 0xFF;

  // On the DollaTek AD9833 module the MCP41010 is not a standalone PGA chip.
  // It forms a variable attenuator into a fixed-gain AD8051 stage, so the board's
  // amplified output is only approximately adjustable from 0x to 6x of the raw DDS VOUT.
  static constexpr double kApproxMinGain = 0.0;
  static constexpr double kApproxMaxGain = 6.0;

  static SpiRleEncoder::Config default_spi_config()
  {
    SpiRleEncoder::Config cfg;
    cfg.decoder_clock_hz = 100e6;
    cfg.spi_clock_hz = 10e6;
    cfg.mode = 3; // MCP41010 samples SI on rising SCK edges; mode 3 keeps the shared clock idle high.
    cfg.bit_order = SpiRleEncoder::BitOrder::MSB_FIRST;
    cfg.bit_sclk = 18;
    cfg.bit_mosi = 17;
    cfg.bit_cs = 16;  // MCP41010 CS, active low
    cfg.bit_aux = 19; // AD9833 FSY/FSYNC, held high so the DDS ignores the shared bus traffic
    cfg.chip_select_active_high = false;
    cfg.aux_default = true;
    cfg.select_setup_ticks = 2;
    cfg.deselect_hold_ticks = 2;
    cfg.post_idle_ticks = 4;
    return cfg;
  }

  explicit Mcp41010(SpiRleEncoder::Config cfg = default_spi_config()) : encoder_(cfg) {}

  void clear() { encoder_.clear(); }

  const std::vector<SpiRleEncoder::Token> &tokens() const { return encoder_.tokens(); }
  double achieved_spi_clock_hz() const { return encoder_.achieved_spi_clock_hz(); }
  uint32_t half_period_ticks() const { return encoder_.half_period_ticks(); }

  static uint8_t command_byte(Command cmd)
  {
    switch (cmd) {
      case Command::Write: return 0x11u;    // C1:C0=01, P0=1 for the single on-chip potentiometer
      case Command::Shutdown: return 0x21u; // C1:C0=10, P0=1
    }
    throw std::invalid_argument("Unknown MCP41010 command");
  }

  static uint16_t command_word(Command cmd, uint8_t data = 0)
  {
    return static_cast<uint16_t>((static_cast<uint16_t>(command_byte(cmd)) << 8) | data);
  }

  static uint16_t wiper_word(uint8_t wiper)
  {
    return command_word(Command::Write, wiper);
  }

  static uint16_t shutdown_word()
  {
    return command_word(Command::Shutdown, 0x00u);
  }

  static double gain_for_wiper(uint8_t wiper)
  {
    return kApproxMaxGain * (static_cast<double>(wiper) / static_cast<double>(kMaxWiper));
  }

  static uint8_t wiper_for_gain(double gain)
  {
    const double clipped = std::clamp(gain, kApproxMinGain, kApproxMaxGain);
    return static_cast<uint8_t>(std::llround((static_cast<double>(kMaxWiper) * clipped) / kApproxMaxGain));
  }

  void write_command(Command cmd, uint8_t data = 0)
  {
    write_word(command_word(cmd, data));
  }

  void write_wiper(uint8_t wiper)
  {
    write_word(wiper_word(wiper));
  }

  void write_gain(double gain)
  {
    write_wiper(wiper_for_gain(gain));
  }

  void shutdown()
  {
    write_word(shutdown_word());
  }

 private:
  SpiRleEncoder encoder_;

  void write_word(uint16_t word)
  {
    encoder_.select();
    encoder_.write_word16(word);
    encoder_.deselect();
  }
};

} // namespace spi_payload
