#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <span>
#include <algorithm>
#include <bitset>
#include <fstream>

class SpiRleEncoder {
 public:
   struct Token {
     uint8_t pins;      // low 4 bits used
     uint32_t repeat;   // number of decoder-clock ticks to hold this state
   };

   enum class BitOrder { MSB_FIRST, LSB_FIRST };

   struct Config {
     double decoder_clock_hz = 100e6;   // RLE decoder clock
     double spi_clock_hz     = 10e6;    // target SPI clock
     int mode                = 0;       // SPI mode: 0..3
     BitOrder bit_order      = BitOrder::MSB_FIRST;

     // Pin bit positions within the 4-bit output nibble; we use the PMOD numbering convention
     int bit_sclk = 3;
     int bit_mosi = 1;
     int bit_cs   = 0;
     int bit_aux  = 2; // miso, not used

     // Chip-select polarity:
     // false -> selected = physical low  (common SPI convention)
     // true  -> selected = physical high
     bool chip_select_active_high = false;

     // Default level of optional auxiliary pin
     bool aux_default = false;

     // Timing around chip select
     uint32_t select_setup_ticks = 1;   // delay after select() before first clock edge
     uint32_t deselect_hold_ticks = 1;  // delay after last clock edge before deselect()
     uint32_t post_idle_ticks = 0;      // optional idle after deselect()
   };

   explicit SpiRleEncoder(Config cfg) : cfg_(cfg) {
     if (cfg_.mode < 0 || cfg_.mode > 3) {
       throw std::invalid_argument("SPI mode must be in range 0..3");
     }
     if (cfg_.decoder_clock_hz <= 0.0 || cfg_.spi_clock_hz <= 0.0) {
       throw std::invalid_argument("Clock frequencies must be positive");
     }
     const double exact_half_period = cfg_.decoder_clock_hz / (2.0 * cfg_.spi_clock_hz);
     half_period_ticks_ = static_cast<uint32_t>(std::llround(exact_half_period));
     if (half_period_ticks_ < 1) {
       half_period_ticks_ = 1;
     }
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

   const std::vector<Token>& tokens() const {
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
     emit_state(
                /*sclk=*/cpol_,
                /*mosi=*/current_mosi_,
                /*selected=*/false,
                /*aux=*/current_aux_,
                ticks
               );
   }

   void select() {
     if (selected_) {
       return;
     }
     selected_ = true;
     emit_state(
                /*sclk=*/cpol_,
                /*mosi=*/current_mosi_,
                /*selected=*/true,
                /*aux=*/current_aux_,
                cfg_.select_setup_ticks
               );
   }

   void deselect() {
     if (!selected_) {
       return;
     }
     emit_state(
                /*sclk=*/cpol_,
                /*mosi=*/current_mosi_,
                /*selected=*/true,
                /*aux=*/current_aux_,
                cfg_.deselect_hold_ticks
               );
        selected_ = false;
     emit_state(
            /*sclk=*/cpol_,
            /*mosi=*/current_mosi_,
            /*selected=*/false,
            /*aux=*/current_aux_,
            cfg_.post_idle_ticks > 0 ? cfg_.post_idle_ticks : 1
        );
    }

   void write_transaction(std::span<const uint8_t> data) {
     select();
     write_bytes(data);
     deselect();
   }

   void write_bytes(std::span<const uint8_t> data) {
     for (uint8_t b : data)
       write_byte(b);
   }

   void write_byte(uint8_t byte) {
     if (cfg_.bit_order == BitOrder::MSB_FIRST) {
       for (int i = 7; i >= 0; --i) {
         write_bit((byte >> i) & 1u);
       }
     } else {
       for (int i = 0; i < 8; ++i) {
         write_bit((byte >> i) & 1u);
       }
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

   // Pack to 16-bit tokens: [15:4] = repeat-1, [3:0] = pins
   std::vector<uint16_t> pack_u16() const {
     std::vector<uint16_t> out;
     out.reserve(tokens_.size());
     for (const auto& t : tokens_) {
       uint32_t remaining = t.repeat;
       while (remaining > 0) {
         const uint32_t chunk = (remaining > 4096u) ? 4096u : remaining;
         const uint16_t word = static_cast<uint16_t>(((chunk - 1u) << 4) | (t.pins & 0x0fu));
         out.push_back(word);
         remaining -= chunk;
       }
     }
     return out;
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

   bool physical_cs_level(bool selected) const {
     // Returns the actual electrical level driven on the chip-select pin.
     return cfg_.chip_select_active_high ? selected : !selected;
   }

   uint8_t compose_pins(bool sclk, bool mosi, bool selected, bool aux) const {
     const bool cs_level = physical_cs_level(selected);
     uint8_t p = 0;
     p |= (static_cast<uint8_t>(sclk)     & 1u) << cfg_.bit_sclk;
     p |= (static_cast<uint8_t>(mosi)     & 1u) << cfg_.bit_mosi;
     p |= (static_cast<uint8_t>(cs_level) & 1u) << cfg_.bit_cs;
     p |= (static_cast<uint8_t>(aux)      & 1u) << cfg_.bit_aux;
     return p & 0x0f;
   }

   void emit_state(bool sclk, bool mosi, bool selected, bool aux, uint32_t ticks) {
     if (ticks == 0)
       return;
     const uint8_t pins = compose_pins(sclk, mosi, selected, aux);
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
     const bool clk_idle   = cpol_;
     const bool clk_active = !cpol_;
     if (!cpha_) {
       // CPHA = 0:
       // data valid during idle half-cycle, then leading edge / active half-cycle
       emit_state(clk_idle,   bit_value, true, current_aux_, half_period_ticks_);
       emit_state(clk_active, bit_value, true, current_aux_, half_period_ticks_);
     } else {
       // CPHA = 1:
       // first half-cycle is at active clock level, second at idle level
       emit_state(clk_active, bit_value, true, current_aux_, half_period_ticks_);
       emit_state(clk_idle,   bit_value, true, current_aux_, half_period_ticks_);
     }
   }
};

static void dump_tokens(const std::vector<SpiRleEncoder::Token>& tokens, std::ostream &F = std::cout) {
  F << "idx repeat pins \n";
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    F << std::setw(4) << std::dec << i
      << "  " << std::dec << tokens[i].repeat
      << "  0x" << std::hex << unsigned(tokens[i].pins)
      << " " << std::bitset<4>(unsigned(tokens[i].pins))
      << "\n";
  }
}

static void dump_tokens_seq(const std::vector<SpiRleEncoder::Token>& tokens, std::ostream &F = std::cout) {
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    F << "d"
      << " " << std::dec << tokens[i].repeat
      << " 0x" << std::hex << unsigned(tokens[i].pins)
      << "\n";
  }
  F << "f" << std::endl;
}

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
  dump_tokens_seq(enc.tokens(), F);
  return 0;
}
