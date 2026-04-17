#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "spi_rle_encoder.hh"

namespace spi_payload {

class Ad9833 {
 public:
  enum class FrequencyReg { Freq0, Freq1 };
  enum class PhaseReg { Phase0, Phase1 };
  enum class Half { Lsb14, Msb14 };
  enum class Waveform { Sine, Triangle, SquareMsb, SquareMsbDiv2 };

  static constexpr double kDefaultMasterClockHz = 25e6;

  static constexpr uint16_t kControlB28 = 1u << 13;
  static constexpr uint16_t kControlHLB = 1u << 12;
  static constexpr uint16_t kControlFselect = 1u << 11;
  static constexpr uint16_t kControlPselect = 1u << 10;
  static constexpr uint16_t kControlReset = 1u << 8;
  static constexpr uint16_t kControlSleep1 = 1u << 7;
  static constexpr uint16_t kControlSleep12 = 1u << 6;
  static constexpr uint16_t kControlOpbiten = 1u << 5;
  static constexpr uint16_t kControlDiv2 = 1u << 3;
  static constexpr uint16_t kControlMode = 1u << 1;

  static constexpr uint16_t kFreq0Prefix = 0x4000u;
  static constexpr uint16_t kFreq1Prefix = 0x8000u;
  static constexpr uint16_t kPhase0Prefix = 0xC000u;
  static constexpr uint16_t kPhase1Prefix = 0xE000u;

  static constexpr uint32_t kFrequency28Mask = 0x0FFFFFFFu;
  static constexpr uint16_t kFrequency14Mask = 0x3FFFu;
  static constexpr uint16_t kPhase12Mask = 0x0FFFu;

  static SpiRleEncoder::Config default_spi_config()
  {
    SpiRleEncoder::Config cfg;
    cfg.decoder_clock_hz = 100e6;
    cfg.spi_clock_hz = 10e6;
    cfg.mode = 2;
    cfg.bit_order = SpiRleEncoder::BitOrder::MSB_FIRST;
    cfg.bit_sclk = 18;
    cfg.bit_mosi = 17;
    cfg.bit_cs = 19;  // AD9833 FSY/FSYNC, active low
    cfg.bit_aux = 16; // MCP41010 CS, held high so the bus only addresses the AD9833
    cfg.chip_select_active_high = false;
    cfg.aux_default = true;
    cfg.select_setup_ticks = 2;
    cfg.deselect_hold_ticks = 2;
    cfg.post_idle_ticks = 4;
    return cfg;
  }

  explicit Ad9833(SpiRleEncoder::Config cfg = default_spi_config(),
                  double master_clock_hz = kDefaultMasterClockHz)
      : encoder_(cfg), master_clock_hz_(master_clock_hz)
  {
    if (master_clock_hz_ <= 0.0)
      throw std::invalid_argument("AD9833 master clock must be positive");
  }

  void clear() { encoder_.clear(); }

  const std::vector<SpiRleEncoder::Token> &tokens() const { return encoder_.tokens(); }
  double achieved_spi_clock_hz() const { return encoder_.achieved_spi_clock_hz(); }
  uint32_t half_period_ticks() const { return encoder_.half_period_ticks(); }
  double master_clock_hz() const { return master_clock_hz_; }
  uint16_t control_word() const { return control_; }

  static uint32_t tuning_word_for_frequency(double output_frequency_hz,
                                            double master_clock_hz = kDefaultMasterClockHz)
  {
    if (output_frequency_hz < 0.0)
      throw std::invalid_argument("AD9833 output frequency must be non-negative");
    if (master_clock_hz <= 0.0)
      throw std::invalid_argument("AD9833 master clock must be positive");

    constexpr double scale = 268435456.0; // 2^28
    const double exact_word = (output_frequency_hz * scale) / master_clock_hz;
    if (exact_word > (scale - 1.0))
      throw std::invalid_argument("AD9833 output frequency exceeds the 28-bit tuning range");
    return static_cast<uint32_t>(std::llround(exact_word));
  }

  uint32_t tuning_word(double output_frequency_hz) const
  {
    return tuning_word_for_frequency(output_frequency_hz, master_clock_hz_);
  }

  static double realized_frequency_hz(uint32_t tuning_word,
                                      double master_clock_hz = kDefaultMasterClockHz)
  {
    validate_frequency28(tuning_word);
    return (static_cast<double>(tuning_word) * master_clock_hz) / 268435456.0;
  }

  double realized_frequency(uint32_t tuning_word) const
  {
    return realized_frequency_hz(tuning_word, master_clock_hz_);
  }

  static uint16_t tuning_word_for_phase_deg(double phase_deg)
  {
    const double turns = std::fmod(phase_deg, 360.0);
    const double normalized = turns < 0.0 ? (turns + 360.0) : turns;
    return static_cast<uint16_t>(std::llround((normalized * 4096.0) / 360.0)) & kPhase12Mask;
  }

  void set_b28(bool enabled) { set_control_bit(kControlB28, enabled); }
  void set_half(Half half) { set_control_bit(kControlHLB, half == Half::Msb14); }
  void select_frequency(FrequencyReg reg) { set_control_bit(kControlFselect, reg == FrequencyReg::Freq1); }
  void select_phase(PhaseReg reg) { set_control_bit(kControlPselect, reg == PhaseReg::Phase1); }
  void set_reset(bool enabled) { set_control_bit(kControlReset, enabled); }
  void sleep_clock(bool enabled) { set_control_bit(kControlSleep1, enabled); }
  void sleep_dac(bool enabled) { set_control_bit(kControlSleep12, enabled); }

  void set_waveform(Waveform waveform)
  {
    control_ &= static_cast<uint16_t>(~(kControlOpbiten | kControlDiv2 | kControlMode));
    switch (waveform) {
      case Waveform::Sine:
        return;
      case Waveform::Triangle:
        control_ |= kControlMode;
        return;
      case Waveform::SquareMsb:
        control_ |= static_cast<uint16_t>(kControlOpbiten | kControlDiv2);
        return;
      case Waveform::SquareMsbDiv2:
        control_ |= kControlOpbiten;
        return;
    }
    throw std::invalid_argument("Unknown AD9833 waveform mode");
  }

  static uint16_t frequency_word(FrequencyReg reg, uint16_t value14)
  {
    validate_frequency14(value14);
    return static_cast<uint16_t>(frequency_prefix(reg) | value14);
  }

  static std::array<uint16_t, 2> frequency_words(FrequencyReg reg, uint32_t tuning_word)
  {
    validate_frequency28(tuning_word);
    return {
        frequency_word(reg, static_cast<uint16_t>(tuning_word & kFrequency14Mask)),
        frequency_word(reg, static_cast<uint16_t>((tuning_word >> 14) & kFrequency14Mask)),
    };
  }

  static uint16_t phase_word(PhaseReg reg, uint16_t value12)
  {
    validate_phase12(value12);
    return static_cast<uint16_t>(phase_prefix(reg) | value12);
  }

  void write_control() { write_word(control_); }

  void write_frequency(FrequencyReg reg, uint32_t tuning_word)
  {
    const auto words = frequency_words(reg, tuning_word);
    write_word(words[0]);
    write_word(words[1]);
  }

  void write_frequency14(FrequencyReg reg, uint16_t value14)
  {
    write_word(frequency_word(reg, value14));
  }

  void write_phase(PhaseReg reg, uint16_t value12)
  {
    write_word(phase_word(reg, value12));
  }

 private:
  SpiRleEncoder encoder_;
  double master_clock_hz_;
  uint16_t control_ = 0;

  static void validate_frequency28(uint32_t tuning_word)
  {
    if ((tuning_word & ~kFrequency28Mask) != 0)
      throw std::invalid_argument("AD9833 frequency tuning word must fit in 28 bits");
  }

  static void validate_frequency14(uint16_t value14)
  {
    if ((value14 & ~kFrequency14Mask) != 0)
      throw std::invalid_argument("AD9833 frequency half-word must fit in 14 bits");
  }

  static void validate_phase12(uint16_t value12)
  {
    if ((value12 & ~kPhase12Mask) != 0)
      throw std::invalid_argument("AD9833 phase word must fit in 12 bits");
  }

  static uint16_t frequency_prefix(FrequencyReg reg)
  {
    return reg == FrequencyReg::Freq0 ? kFreq0Prefix : kFreq1Prefix;
  }

  static uint16_t phase_prefix(PhaseReg reg)
  {
    return reg == PhaseReg::Phase0 ? kPhase0Prefix : kPhase1Prefix;
  }

  void set_control_bit(uint16_t mask, bool enabled)
  {
    if (enabled)
      control_ |= mask;
    else
      control_ &= static_cast<uint16_t>(~mask);
  }

  void write_word(uint16_t word)
  {
    encoder_.select();
    encoder_.write_word16(word);
    encoder_.deselect();
  }
};

} // namespace spi_payload
