#pragma once

#include <cmath>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace spi_payload {

class SpiRleEncoder {
 public:
  static constexpr int kOutputWidth = 32;

  struct Token {
    uint32_t pins = 0;
    uint32_t repeat = 0;
  };

  enum class BitOrder { MSB_FIRST, LSB_FIRST };

  struct Config {
    double decoder_clock_hz = 100e6;
    double spi_clock_hz = 10e6;
    int mode = 0;
    BitOrder bit_order = BitOrder::MSB_FIRST;

    int bit_sclk = 3;
    int bit_mosi = 1;
    int bit_cs = 0;
    int bit_aux = 2;

    bool chip_select_active_high = false;
    bool aux_default = false;

    uint32_t select_setup_ticks = 1;
    uint32_t deselect_hold_ticks = 1;
    uint32_t post_idle_ticks = 0;
  };

  explicit SpiRleEncoder(Config cfg) : cfg_(cfg) {
    validate_config();
    const double exact_half_period = cfg_.decoder_clock_hz / (2.0 * cfg_.spi_clock_hz);
    half_period_ticks_ = static_cast<uint32_t>(std::llround(exact_half_period));
    if (half_period_ticks_ < 1)
      half_period_ticks_ = 1;
    cpol_ = (cfg_.mode == 2 || cfg_.mode == 3);
    cpha_ = (cfg_.mode == 1 || cfg_.mode == 3);
    clear();
  }

  void clear() {
    tokens_.clear();
    selected_ = false;
    current_mosi_ = false;
    current_aux_ = cfg_.aux_default;
    emit_idle(1);
  }

  const std::vector<Token> &tokens() const {
    return tokens_;
  }

  double achieved_spi_clock_hz() const {
    return cfg_.decoder_clock_hz / (2.0 * static_cast<double>(half_period_ticks_));
  }

  uint32_t half_period_ticks() const {
    return half_period_ticks_;
  }

  bool is_selected() const {
    return selected_;
  }

  void set_aux(bool value) {
    current_aux_ = value;
  }

  void emit_idle(uint32_t ticks) {
    emit_state(cpol_, current_mosi_, false, current_aux_, ticks);
  }

  void select() {
    if (selected_)
      return;
    selected_ = true;
    emit_state(cpol_, current_mosi_, true, current_aux_, cfg_.select_setup_ticks);
  }

  void deselect() {
    if (!selected_)
      return;
    emit_state(cpol_, current_mosi_, true, current_aux_, cfg_.deselect_hold_ticks);
    selected_ = false;
    emit_state(cpol_, current_mosi_, false, current_aux_, cfg_.post_idle_ticks > 0 ? cfg_.post_idle_ticks : 1);
  }

  void write_transaction(std::span<const uint8_t> data) {
    select();
    write_bytes(data);
    deselect();
  }

  void write_bytes(std::span<const uint8_t> data) {
    for (const uint8_t byte : data)
      write_byte(byte);
  }

  void write_byte(uint8_t byte) {
    if (cfg_.bit_order == BitOrder::MSB_FIRST) {
      for (int i = 7; i >= 0; --i)
        write_bit((byte >> i) & 1u);
    } else {
      for (int i = 0; i < 8; ++i)
        write_bit((byte >> i) & 1u);
    }
  }

  void write_word16(uint16_t word) {
    if (cfg_.bit_order == BitOrder::MSB_FIRST) {
      write_byte(static_cast<uint8_t>((word >> 8) & 0xff));
      write_byte(static_cast<uint8_t>(word & 0xff));
    } else {
      write_byte(static_cast<uint8_t>(word & 0xff));
      write_byte(static_cast<uint8_t>((word >> 8) & 0xff));
    }
  }

 private:
  Config cfg_;
  std::vector<Token> tokens_;

  uint32_t half_period_ticks_ = 1;
  bool cpol_ = false;
  bool cpha_ = false;
  bool selected_ = false;
  bool current_mosi_ = false;
  bool current_aux_ = false;

  static void validate_bit_index(int idx, const char *name) {
    if (idx < 0 || idx >= kOutputWidth)
      throw std::invalid_argument(std::string("SPI pin index out of range for ") + name);
  }

  void validate_config() const {
    if (cfg_.mode < 0 || cfg_.mode > 3)
      throw std::invalid_argument("SPI mode must be in range 0..3");
    if (cfg_.decoder_clock_hz <= 0.0 || cfg_.spi_clock_hz <= 0.0)
      throw std::invalid_argument("Clock frequencies must be positive");

    validate_bit_index(cfg_.bit_sclk, "SCLK");
    validate_bit_index(cfg_.bit_mosi, "MOSI");
    validate_bit_index(cfg_.bit_cs, "CS");
    validate_bit_index(cfg_.bit_aux, "AUX");

    if (cfg_.bit_sclk == cfg_.bit_mosi || cfg_.bit_sclk == cfg_.bit_cs || cfg_.bit_sclk == cfg_.bit_aux ||
        cfg_.bit_mosi == cfg_.bit_cs || cfg_.bit_mosi == cfg_.bit_aux || cfg_.bit_cs == cfg_.bit_aux) {
      throw std::invalid_argument("SPI pin indices must be distinct");
    }
  }

  bool physical_cs_level(bool selected) const {
    return cfg_.chip_select_active_high ? selected : !selected;
  }

  uint32_t compose_pins(bool sclk, bool mosi, bool selected, bool aux) const {
    uint32_t pins = 0;
    pins |= (static_cast<uint32_t>(sclk) & 1u) << cfg_.bit_sclk;
    pins |= (static_cast<uint32_t>(mosi) & 1u) << cfg_.bit_mosi;
    pins |= (static_cast<uint32_t>(physical_cs_level(selected)) & 1u) << cfg_.bit_cs;
    pins |= (static_cast<uint32_t>(aux) & 1u) << cfg_.bit_aux;
    return pins;
  }

  void emit_state(bool sclk, bool mosi, bool selected, bool aux, uint32_t ticks) {
    if (ticks == 0)
      return;
    const uint32_t pins = compose_pins(sclk, mosi, selected, aux);
    if (!tokens_.empty() && tokens_.back().pins == pins) {
      tokens_.back().repeat += ticks;
    } else {
      tokens_.push_back(Token{pins, ticks});
    }
    current_mosi_ = mosi;
  }

  void write_bit(bool bit_value) {
    if (!selected_)
      throw std::runtime_error("write_bit called while chip select is inactive");
    const bool clk_idle = cpol_;
    const bool clk_active = !cpol_;
    if (!cpha_) {
      emit_state(clk_idle, bit_value, true, current_aux_, half_period_ticks_);
      emit_state(clk_active, bit_value, true, current_aux_, half_period_ticks_);
    } else {
      emit_state(clk_active, bit_value, true, current_aux_, half_period_ticks_);
      emit_state(clk_idle, bit_value, true, current_aux_, half_period_ticks_);
    }
  }
};

inline void dump_tokens(const std::vector<SpiRleEncoder::Token> &tokens, std::ostream &stream = std::cout) {
  stream << "idx repeat pins\n";
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    stream << std::setw(4) << std::dec << i
           << "  " << std::dec << tokens[i].repeat
           << "  0x" << std::hex << tokens[i].pins
           << " " << std::bitset<SpiRleEncoder::kOutputWidth>(static_cast<unsigned long long>(tokens[i].pins))
           << "\n";
  }
}

inline void dump_tokens_seq(const std::vector<SpiRleEncoder::Token> &tokens, std::ostream &stream = std::cout) {
  for (const auto &token : tokens)
    stream << "d " << std::dec << token.repeat << " 0x" << std::hex << token.pins << "\n";
  stream << "f\n";
}

} // namespace spi_payload
