#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "spi_rle_encoder.hh"

using spi_payload::SpiRleEncoder;

static constexpr double kAd9833MasterClockHz = 25e6;
static constexpr double kTargetOutputFrequencyHz = 5e6;

static uint32_t ad9833_frequency_word(double output_frequency_hz,
                                      double master_clock_hz = kAd9833MasterClockHz)
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

static double ad9833_realized_frequency_hz(uint32_t frequency_word,
                                           double master_clock_hz = kAd9833MasterClockHz)
{
  constexpr double scale = 268435456.0; // 2^28
  return (static_cast<double>(frequency_word) * master_clock_hz) / scale;
}

static uint16_t ad9833_freq0_lsb_word(uint32_t frequency_word)
{
  return static_cast<uint16_t>(0x4000u | (frequency_word & 0x3fffu));
}

static uint16_t ad9833_freq0_msb_word(uint32_t frequency_word)
{
  return static_cast<uint16_t>(0x4000u | ((frequency_word >> 14) & 0x3fffu));
}

static std::array<uint16_t, 5> ad9833_sine_program(uint32_t frequency_word)
{
  return {
      0x2100u,
      ad9833_freq0_lsb_word(frequency_word),
      ad9833_freq0_msb_word(frequency_word),
      0xC000u,
      0x2000u,
  };
}

int main()
{
  SpiRleEncoder::Config cfg;
  cfg.decoder_clock_hz = 100e6;
  cfg.spi_clock_hz = 10e6;
  cfg.mode = 2;
  cfg.bit_order = SpiRleEncoder::BitOrder::MSB_FIRST;
  cfg.bit_sclk = 18;
  cfg.bit_mosi = 17;
  cfg.bit_cs = 19;  // AD9833 FSY/FSYNC, active low
  cfg.bit_aux = 16; // MCP41010 CS, held high so the shared bus only addresses the AD9833
  cfg.chip_select_active_high = false;
  cfg.aux_default = true;
  cfg.select_setup_ticks = 2;
  cfg.deselect_hold_ticks = 2;
  cfg.post_idle_ticks = 4;

  const uint32_t frequency_word = ad9833_frequency_word(kTargetOutputFrequencyHz);
  const double realized_output_frequency_hz = ad9833_realized_frequency_hz(frequency_word);
  const auto program = ad9833_sine_program(frequency_word);

  SpiRleEncoder enc(cfg);
  for (const uint16_t word : program) {
    enc.select();
    enc.write_word16(word);
    enc.deselect();
  }

  std::cout << "Requested SPI clock:    " << cfg.spi_clock_hz << " Hz\n";
  std::cout << "Achieved  SPI clock:    " << enc.achieved_spi_clock_hz() << " Hz\n";
  std::cout << "Half-period ticks:      " << enc.half_period_ticks() << "\n";
  std::cout << "AD9833 master clock:    " << kAd9833MasterClockHz << " Hz\n";
  std::cout << "Requested output freq:  " << kTargetOutputFrequencyHz << " Hz\n";
  std::cout << "Realized  output freq:  " << std::fixed << std::setprecision(9) << realized_output_frequency_hz << " Hz\n";
  std::cout << "Frequency register:     0x" << std::hex << frequency_word << std::dec << "\n";
  std::cout << "Programming words:      ";
  for (const uint16_t word : program)
    std::cout << "0x" << std::hex << word << " ";
  std::cout << std::dec << "\n\n";

  spi_payload::dump_tokens(enc.tokens());

  std::cout << "qout[19]=FSY qout[18]=CLK qout[17]=DAT qout[16]=CS(MCP41010)" << std::endl;

  std::ofstream sequence_file("sequence");
  spi_payload::dump_tokens_seq(enc.tokens(), sequence_file);
  return 0;
}
