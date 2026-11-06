#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "spi_rle_encoder.hh"

namespace spi_payload {

// AD9959 module on PP_PMOD J3/J4, using its onboard 25 MHz reference.
// Register definitions: AD9959 Rev. C, Tables 28, 29 and 31-37.
class Ad9959 {
 public:
  static constexpr double kReferenceClockHz = 25e6;
  static constexpr double kSystemClockHz = 500e6;
  static constexpr uint32_t kIdlePins = 1u << 1;
  static constexpr uint32_t kResetPins = kIdlePins | (1u << 7);
  static constexpr std::array<double, 4> kDefaultFrequenciesHz = {50e6, 75e6, 100e6, 125e6};

  static SpiRleEncoder::Config default_spi_config()
  {
    SpiRleEncoder::Config cfg;
    cfg.decoder_clock_hz = 100e6;
    cfg.spi_clock_hz = 12.5e6;
    cfg.mode = 0;
    cfg.bit_sclk = 0;
    cfg.bit_cs = 1;
    cfg.bit_mosi = 2;
    cfg.bit_aux = 6; // I/O_UPDATE
    cfg.select_setup_ticks = 2;
    cfg.deselect_hold_ticks = 2;
    cfg.post_idle_ticks = 4;
    return cfg;
  }

  static uint32_t tuning_word_for_frequency(double frequency_hz)
  {
    if (!std::isfinite(frequency_hz) || frequency_hz <= 0.0 || frequency_hz > 200e6)
      throw std::invalid_argument("AD9959 module frequency must be finite and in (0, 200 MHz]");
    const auto word = static_cast<uint32_t>(std::llround(frequency_hz * 4294967296.0 / kSystemClockHz));
    if (word == 0)
      throw std::invalid_argument("AD9959 frequency rounds to DC at a 500 MHz system clock");
    return word;
  }

  static double realized_frequency_hz(uint32_t word)
  {
    return static_cast<double>(word) * kSystemClockHz / 4294967296.0;
  }

  static std::vector<SpiRleEncoder::Token> tone_sequence(
      const std::array<double, 4> &frequencies_hz = kDefaultFrequenciesHz)
  {
    std::array<uint32_t, 4> words;
    for (std::size_t channel = 0; channel < words.size(); ++channel)
      words[channel] = tuning_word_for_frequency(frequencies_hz[channel]);

    // All counts are 100 MHz streamer ticks. Reset and UPDATE need at least
    // 160 ns before enabling the PLL; use 1 us pulses and reset recovery.
    std::vector<SpiRleEncoder::Token> result = {
        {kIdlePins, 100}, {kResetPins, 100}, {kIdlePins, 100}};
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x01, 0xD00000, 3); // VCO high range, PLL x20
    update(spi);
    spi.emit_idle(200000); // 2 ms PLL settling (datasheet: typically 1 ms)

    // Reset selects every channel and 1-bit, 2-wire, MSB-first serial mode.
    // SDIO_3 remains low (SYNC_I/O); P0-P3 and PWR_DWN_CTL also stay low.
    write_register(spi, 0x00, 0xF0, 1);
    write_register(spi, 0x03, 0x000301, 3); // single tone, full DAC current, sine
    write_register(spi, 0x05, 0x0000, 2);   // zero phase offset
    write_register(spi, 0x06, 0x0013FF, 3); // manual amplitude scale, 1023/1024
    for (std::size_t channel = 0; channel < words.size(); ++channel) {
      write_register(spi, 0x00, 0x10u << channel, 1);
      write_register(spi, 0x04, words[channel], 4);
    }
    write_register(spi, 0x00, 0xF0, 1); // select all; these bits gate writes, not RF
    update(spi); // commit every channel together
    append_tokens(result, spi.tokens());
    return result;
  }

 private:
  static void write_register(SpiRleEncoder &spi, uint8_t address, uint32_t value, unsigned bytes)
  {
    std::vector<uint8_t> data = {address}; // write instruction: bit 7 = 0
    for (unsigned i = bytes; i > 0; --i)
      data.push_back(static_cast<uint8_t>(value >> (8 * (i - 1))));
    spi.write_transaction(data);
  }

  static void update(SpiRleEncoder &spi)
  {
    spi.set_aux(true);
    spi.emit_idle(100);
    spi.set_aux(false);
    spi.emit_idle(100);
  }
};

} // namespace spi_payload
