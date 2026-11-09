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
      const std::array<double, 4> &frequencies_hz = kDefaultFrequenciesHz, uint32_t post_write_idle_ticks = 4)
  {
    std::array<uint32_t, 4> words;
    for (std::size_t channel = 0; channel < words.size(); ++channel)
      words[channel] = tuning_word_for_frequency(frequencies_hz[channel]);

    // All counts are 100 MHz streamer ticks. Reset and UPDATE need at least
    // 160 ns before enabling the PLL; use 1 us pulses and reset recovery.
    std::vector<SpiRleEncoder::Token> result = {
        {kIdlePins, 100}, {kResetPins, 100}, {kIdlePins, 100}};
    auto cfg = default_spi_config();
    cfg.post_idle_ticks = post_write_idle_ticks;
    SpiRleEncoder spi(cfg);
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

  // Update an initialized DDS without resetting its PLL or the other channels.
  static std::vector<SpiRleEncoder::Token> channel1_sequence(double frequency_hz, uint32_t amplitude_code)
  {
    const auto word = frequency_hz == 0.0 ? 0 : tuning_word_for_frequency(frequency_hz);
    if (amplitude_code > 1023)
      throw std::invalid_argument("AD9959 amplitude code must be in 0..1023");
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1);
    write_register(spi, 0x03, 0x000301, 3); // single tone, full DAC current, sine
    write_register(spi, 0x05, 0x0000, 2);
    write_register(spi, 0x04, word, 4);
    write_register(spi, 0x06, 0x001000 | amplitude_code, 3);
    update(spi); // apply frequency and amplitude together
    write_register(spi, 0x00, 0xF0, 1);
    return spi.tokens();
  }

  static std::vector<SpiRleEncoder::Token> frequency_staircase_sequence()
  {
    auto result = tone_sequence();
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1); // subsequent writes affect CH1 only
    // Six four-second cycles fit the board's finite-playback input buffers.
    for (unsigned cycle = 0; cycle < 6; ++cycle) {
      for (const double frequency : kDefaultFrequenciesHz) {
        write_register(spi, 0x04, tuning_word_for_frequency(frequency), 4);
        update(spi);
        spi.emit_idle(100000000); // 1 s dwell, in addition to programming time
      }
    }
    write_register(spi, 0x04, tuning_word_for_frequency(75e6), 4);
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    append_tokens(result, spi.tokens());
    return result;
  }

  static std::vector<SpiRleEncoder::Token> two_level_fsk_sequence()
  {
    auto result = tone_sequence();
    result.back().repeat += 1000000; // 10 ms for readback to drain before another SPI burst
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1); // CH1
    write_register(spi, 0x03, 0x800301, 3); // AFP=frequency, sweep disabled
    write_register(spi, 0x04, tuning_word_for_frequency(70e6), 4);
    write_register(spi, 0x0A, tuning_word_for_frequency(80e6), 4); // CW1
    update(spi);
    append_tokens(result, spi.tokens());

    // FR1 remains in two-level mode. P1 (Q9) selects CH1's CFTW0/CW1;
    // no SPI transactions or UPDATE pulses are needed during modulation.
    for (unsigned cycle = 0; cycle < 24; ++cycle) {
      result.push_back({kIdlePins, 50000000});
      result.push_back({kIdlePins | (1u << 9), 50000000});
    }

    spi.clear(); // return P1 low before restoring single-tone operation
    write_register(spi, 0x03, 0x000301, 3);
    write_register(spi, 0x04, tuning_word_for_frequency(75e6), 4);
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    append_tokens(result, spi.tokens());
    return result;
  }

  static std::vector<SpiRleEncoder::Token> sixteen_level_fsk_sequence()
  {
    auto result = tone_sequence();
    result.back().repeat += 1000000; // 10 ms readback drain interval
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1); // CH1
    write_register(spi, 0x01, 0xD01300, 3); // PLL x20, PPC=CH1, 16 levels, no RU/RD
    write_register(spi, 0x03, 0x800301, 3); // frequency modulation, sweep disabled
    write_register(spi, 0x04, tuning_word_for_frequency(60e6), 4);
    for (unsigned profile = 1; profile < 16; ++profile) {
      write_register(spi, static_cast<uint8_t>(0x09 + profile),
                     tuning_word_for_frequency(60e6 + 2e6 * profile), 4);
      if (profile % 4 == 0)
        spi.emit_idle(1000000); // drain readback between groups of profile writes
    }
    update(spi);
    append_tokens(result, spi.tokens());

    constexpr std::array<unsigned, 16> order = {0, 7, 2, 13, 5, 10, 1, 15, 8, 3, 12, 6, 14, 4, 11, 9};
    for (unsigned cycle = 0; cycle < 6; ++cycle) {
      for (const unsigned profile : order) {
        uint32_t pins = kIdlePins;
        // Datasheet Table 12: [P0:P3] is MSB first. P0..P3 are Q8..Q11.
        for (unsigned pin = 0; pin < 4; ++pin)
          pins |= ((profile >> (3 - pin)) & 1u) << (8 + pin);
        result.push_back({pins, 25000000}); // 250 ms per profile
      }
    }

    spi.clear(); // profile zero while restoring the standard single-tone setup
    write_register(spi, 0x01, 0xD00000, 3);
    write_register(spi, 0x03, 0x000301, 3);
    write_register(spi, 0x04, tuning_word_for_frequency(75e6), 4);
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    append_tokens(result, spi.tokens());
    return result;
  }

  static std::vector<SpiRleEncoder::Token> asymmetric_sweep_sequence()
  {
    auto result = tone_sequence();
    result.back().repeat += 1000000; // 10 ms readback drain interval
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1); // CH1
    write_register(spi, 0x03, 0x806301, 3); // frequency sweep, load SRR on UPDATE, dwell enabled
    write_register(spi, 0x04, tuning_word_for_frequency(60e6), 4);
    write_register(spi, 0x0A, tuning_word_for_frequency(90e6), 4);
    write_register(spi, 0x07, 0xFAFA, 2); // both timers: 250 SYNC_CLK cycles = 2 us
    write_register(spi, 0x08, 516, 4); // RDW: about 60.0703 Hz per step
    write_register(spi, 0x09, 172, 4); // FDW: about 20.0234 Hz per step
    update(spi);
    append_tokens(result, spi.tokens());

    // The hardware reaches each endpoint slightly before the next P1 edge:
    // 0.998830 s up, 2.996490 s down. P1 high/low chooses the direction.
    for (unsigned cycle = 0; cycle < 6; ++cycle) {
      result.push_back({kIdlePins | (1u << 9), 100000000});
      result.push_back({kIdlePins, 300000000});
    }

    spi.clear();
    write_register(spi, 0x03, 0x000301, 3);
    write_register(spi, 0x04, tuning_word_for_frequency(75e6), 4);
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    append_tokens(result, spi.tokens());
    return result;
  }

  static std::vector<SpiRleEncoder::Token> no_dwell_sweep_sequence()
  {
    auto result = tone_sequence();
    result.back().repeat += 1000000; // 10 ms readback drain interval
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1); // CH1
    write_register(spi, 0x03, 0x80E301, 3); // frequency sweep, load SRR on UPDATE, no dwell
    write_register(spi, 0x04, tuning_word_for_frequency(60e6), 4);
    write_register(spi, 0x0A, tuning_word_for_frequency(90e6), 4);
    write_register(spi, 0x07, 0xFAFA, 2); // rising timer: 250 SYNC_CLK cycles = 2 us
    write_register(spi, 0x08, 1032, 4); // about 120.1406 Hz per step; 0.499416 s sweep
    update(spi);
    append_tokens(result, spi.tokens());

    // No-dwell returns to 60 MHz even while P1 remains high. A fresh rising
    // edge is required to start each subsequent sweep; FDW/FSRR are unused.
    for (unsigned cycle = 0; cycle < 24; ++cycle) {
      result.push_back({kIdlePins | (1u << 9), 75000000});
      result.push_back({kIdlePins, 25000000});
    }

    spi.clear();
    write_register(spi, 0x03, 0x000301, 3);
    write_register(spi, 0x04, tuning_word_for_frequency(75e6), 4);
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    append_tokens(result, spi.tokens());
    return result;
  }

  static std::vector<SpiRleEncoder::Token> amplitude_staircase_sequence()
  {
    auto result = tone_sequence();
    result.back().repeat += 1000000; // 10 ms readback drain interval
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1); // CH1; frequency remains 75 MHz
    constexpr std::array<uint32_t, 5> scales = {1023, 512, 256, 128, 64};
    for (unsigned cycle = 0; cycle < 5; ++cycle) {
      for (const uint32_t scale : scales) {
        write_register(spi, 0x06, 0x001000 | scale, 3); // manual amplitude multiplier
        update(spi);
        spi.emit_idle(100000000); // 1 s per level, plus programming time
      }
    }

    write_register(spi, 0x06, 0x0013FF, 3); // restore full digital amplitude
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    append_tokens(result, spi.tokens());
    return result;
  }

  static std::vector<SpiRleEncoder::Token> dac_current_comparison_sequence()
  {
    auto result = tone_sequence();
    result.back().repeat += 1000000; // 10 ms readback drain interval
    SpiRleEncoder spi(default_spi_config());
    write_register(spi, 0x00, 0x20, 1); // CH1; fixed 75 MHz
    // Table 5: current codes are full=11, half=01, quarter=10, eighth=00.
    constexpr std::array<uint32_t, 4> current_settings = {0x301, 0x101, 0x201, 0x001};
    constexpr std::array<uint32_t, 4> digital_scales = {1023, 512, 256, 128};
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
      write_register(spi, 0x06, 0x0013FF, 3); // full digital scale during current comparison
      for (const uint32_t cfr : current_settings) {
        write_register(spi, 0x03, cfr, 3);
        update(spi);
        spi.emit_idle(100000000); // 1 s per current setting, plus programming time
      }
      write_register(spi, 0x03, 0x000301, 3); // full current during digital comparison
      for (const uint32_t scale : digital_scales) {
        write_register(spi, 0x06, 0x001000 | scale, 3);
        update(spi);
        spi.emit_idle(100000000);
      }
    }

    write_register(spi, 0x06, 0x0013FF, 3);
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    append_tokens(result, spi.tokens());
    return result;
  }

  struct ProfileSequence {
    std::vector<SpiRleEncoder::Token> setup;
    std::vector<SpiRleEncoder::Token> period;
    uint32_t repetitions = 0;
    unsigned blocks = 1;
    uint32_t idle_ticks = 0;
    std::vector<std::vector<SpiRleEncoder::Token>> block_setup;
    std::vector<SpiRleEncoder::Token> restore;
  };

  static ProfileSequence sinusoidal_am_sequence()
  {
    ProfileSequence result;
    result.period.resize(8);
    result.repetitions = 24000;
    // Verbose -read capture needs time to format and write every SPI edge.
    auto cfg = default_spi_config();
    cfg.post_idle_ticks = 1000000; // 10 ms after every SPI transaction
    result.setup = tone_sequence(kDefaultFrequenciesHz, cfg.post_idle_ticks);
    SpiRleEncoder spi(cfg);
    write_register(spi, 0x00, 0x20, 1); // CH1, still at 75 MHz
    write_register(spi, 0x01, 0xD01200, 3); // eight-level modulation, PPC=CH1, no RU/RD
    write_register(spi, 0x03, 0x400301, 3); // amplitude modulation, sweep disabled
    for (unsigned sample = 0; sample < result.period.size(); ++sample) {
      const double phase = 2 * std::acos(-1.0) * sample / result.period.size();
      const auto scale = static_cast<uint32_t>(std::llround(682.0 + 341.0 * std::cos(phase)));
      if (sample == 0)
        write_register(spi, 0x06, 0x001000 | scale, 3); // profile zero in ACR
      else
        write_register(spi, static_cast<uint8_t>(0x09 + sample), scale << 22, 4);
      uint32_t pins = kIdlePins;
      // Eight-level mode: [P0:P2] is MSB first (Q8..Q10); P3 remains low.
      for (unsigned pin = 0; pin < 3; ++pin)
        pins |= ((sample >> (2 - pin)) & 1u) << (8 + pin);
      result.period[sample] = {pins, 12500}; // 125 us/sample, 1 ms/period
    }
    update(spi);
    append_tokens(result.setup, spi.tokens());

    spi.clear(); // profile zero restores the peak amplitude before SPI cleanup
    write_register(spi, 0x01, 0xD00000, 3);
    write_register(spi, 0x03, 0x000301, 3);
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    result.restore = spi.tokens();
    return result;
  }

  static ProfileSequence phase_keying_sequence()
  {
    ProfileSequence result;
    result.period = {{kIdlePins, 5000}, {kIdlePins | (1u << 9), 5000}};
    result.repetitions = 30000; // 3 s of 0/180 degree phase switching at 10 kHz
    result.blocks = 4;
    result.idle_ticks = 300000000; // 3 s plain carrier after each PSK block
    auto cfg = default_spi_config();
    cfg.post_idle_ticks = 1000000; // verbose readback drain after every SPI write
    result.setup = tone_sequence(kDefaultFrequenciesHz, cfg.post_idle_ticks);
    SpiRleEncoder spi(cfg);
    write_register(spi, 0x00, 0x20, 1); // CH1; FR1 remains in two-level mode
    write_register(spi, 0x03, 0xC00301, 3); // phase modulation, sweep disabled
    write_register(spi, 0x05, 0x0000, 2); // P1 low: zero phase offset
    write_register(spi, 0x0A, 0x80000000, 4); // P1 high: 180 degrees, MSB-aligned 14-bit word
    update(spi);
    append_tokens(result.setup, spi.tokens());

    spi.clear();
    write_register(spi, 0x03, 0x000301, 3); // restore single-tone mode
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    result.restore = spi.tokens();
    return result;
  }

  static ProfileSequence burst_comparison_sequence()
  {
    ProfileSequence result;
    result.period = {{kIdlePins | (1u << 9), 1000000}, {kIdlePins, 1000000}};
    result.repetitions = 250; // 5 s of 10 ms on / 10 ms off
    result.blocks = 4; // abrupt, ramped, abrupt, ramped
    auto cfg = default_spi_config();
    cfg.post_idle_ticks = 1000000; // readback drain after every SPI write
    result.setup = tone_sequence(kDefaultFrequenciesHz, cfg.post_idle_ticks);
    SpiRleEncoder spi(cfg);
    write_register(spi, 0x00, 0x20, 1); // CH1 remains at 75 MHz
    write_register(spi, 0x0A, 0xFFC00000, 4); // abrupt mode: P1 high selects full amplitude
    append_tokens(result.setup, spi.tokens());

    for (unsigned block = 0; block < result.blocks; ++block) {
      spi.clear(); // P1 low between blocks
      const bool ramped = block % 2 != 0;
      write_register(spi, 0x03, ramped ? 0x000301 : 0x400301, 3);
      // Abrupt: two-level ASK, profile zero muted. Ramped: single-tone auto
      // RU/RD, 1 LSB per 61 SYNC_CLK cycles; 1023 steps take about 0.499 ms.
      write_register(spi, 0x06, ramped ? 0x3D1FFF : 0x001000, 3);
      update(spi);
      result.block_setup.push_back(spi.tokens());
    }

    spi.clear();
    write_register(spi, 0x03, 0x000301, 3);
    write_register(spi, 0x06, 0x0013FF, 3); // manual full amplitude, auto ramp disabled
    update(spi);
    write_register(spi, 0x00, 0xF0, 1);
    result.restore = spi.tokens();
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
