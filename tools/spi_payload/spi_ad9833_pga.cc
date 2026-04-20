#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "ad9833.hh"
#include "mcp41010.hh"
#include "spi_rle_encoder.hh"

using spi_payload::Ad9833;
using spi_payload::Mcp41010;

namespace {

constexpr double kTargetOutputFrequencyHz = 5e6;
constexpr double kTargetApproxGain = 5.0;

void print_word(uint16_t word, const char *meaning)
{
  std::cout << "0x" << std::hex << std::setw(4) << std::setfill('0') << word
            << std::setfill(' ') << std::dec << " = " << meaning << "\n";
}

void print_word32(uint32_t word, const char *meaning)
{
  std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << word
            << std::setfill(' ') << std::dec << " = " << meaning << "\n";
}

} // namespace

int main()
{
  Ad9833 dds;
  const uint32_t freq0_tuning_word = Ad9833::tuning_word_for_frequency(kTargetOutputFrequencyHz, dds.master_clock_hz());
  const double realized_output_frequency_hz = Ad9833::realized_frequency_hz(freq0_tuning_word, dds.master_clock_hz());

  dds.set_b28(true);
  dds.select_frequency(Ad9833::FrequencyReg::Freq0);
  dds.select_phase(Ad9833::PhaseReg::Phase0);
  dds.set_waveform(Ad9833::Waveform::Sine);

  dds.set_reset(true);
  const uint16_t control_reset = dds.control_word();
  const auto freq0_words = Ad9833::frequency_words(Ad9833::FrequencyReg::Freq0, freq0_tuning_word);
  const uint16_t phase0_zero = Ad9833::phase_word(Ad9833::PhaseReg::Phase0, 0x000u);

  dds.set_reset(false);
  const uint16_t control_run = dds.control_word();

  dds.set_reset(true);
  dds.write_control();
  dds.write_frequency(Ad9833::FrequencyReg::Freq0, freq0_tuning_word);
  dds.write_phase(Ad9833::PhaseReg::Phase0, 0x000u);
  dds.set_reset(false);
  dds.write_control();

  Mcp41010 pga;
  // This board's "gain" control is approximate: the MCP41010 attenuates the DDS output
  // ahead of a fixed AD8051 stage, so the practical amplified-output range is about 0x to 6x.
  const uint8_t pga_wiper = Mcp41010::wiper_for_gain(kTargetApproxGain);
  const double realized_approx_gain = Mcp41010::gain_for_wiper(pga_wiper);
  const uint16_t pga_word = Mcp41010::wiper_word(pga_wiper);
  pga.write_wiper(pga_wiper);

  std::vector<spi_payload::SpiRleEncoder::Token> tokens;
  spi_payload::append_tokens(tokens, dds.tokens());
  spi_payload::append_tokens(tokens, pga.tokens());

  std::cout << "AD9833 requested SPI clock: " << Ad9833::default_spi_config().spi_clock_hz << " Hz\n";
  std::cout << "AD9833 achieved  SPI clock: " << dds.achieved_spi_clock_hz() << " Hz\n";
  std::cout << "PGA    requested SPI clock: " << Mcp41010::default_spi_config().spi_clock_hz << " Hz\n";
  std::cout << "PGA    achieved  SPI clock: " << pga.achieved_spi_clock_hz() << " Hz\n";
  std::cout << "AD9833 master clock:        " << dds.master_clock_hz() << " Hz\n";
  std::cout << "Requested output freq:      " << kTargetOutputFrequencyHz << " Hz\n";
  std::cout << "Realized  output freq:      " << std::fixed << std::setprecision(9) << realized_output_frequency_hz << " Hz\n";
  std::cout << "PGA wiper range:            0x00..0xff\n";
  std::cout << "PGA approx gain range:      ~0x to ~6x raw AD9833 output\n";
  std::cout << std::setprecision(3);
  std::cout << "Requested PGA gain:         ~" << kTargetApproxGain << "x\n";
  std::cout << "Realized  PGA gain:         ~" << realized_approx_gain << "x\n";

  print_word32(freq0_tuning_word, "round(f_out * 2^28 / MCLK) for 5 MHz on a 25 MHz AD9833 clock");
  print_word(control_reset, "control(B28 | RESET)");
  print_word(freq0_words[0], "FREQ0 low14(tuning_word)");
  print_word(freq0_words[1], "FREQ0 high14(tuning_word)");
  print_word(phase0_zero, "PHASE0(0)");
  print_word(control_run, "control(B28, waveform=sine, FREQ0, PHASE0)");
  print_word(pga_wiper, "round(0xff * 5x / 6x) for the module's approximate amplified-output range");
  print_word(pga_word, "MCP41010 write(P0, wiper=0xd5) for approx 5x amplified output");

  std::cout << "Programming words:          ";
  for (const uint16_t word : {control_reset, freq0_words[0], freq0_words[1], phase0_zero, control_run, pga_word})
    std::cout << "0x" << std::hex << word << " ";
  std::cout << std::dec << "\n\n";

  spi_payload::dump_tokens(tokens);

  std::cout << "qout[19]=FSY qout[18]=CLK qout[17]=DAT qout[16]=CS(MCP41010)" << std::endl;

  std::ofstream sequence_file("sequence");
  spi_payload::dump_tokens_seq(tokens, sequence_file);
  return 0;
}
