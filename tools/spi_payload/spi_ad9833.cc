#include <fstream>
#include <iomanip>
#include <iostream>

#include "ad9833.hh"

using spi_payload::Ad9833;

namespace {

constexpr double kTargetOutputFrequencyHz = 5e6;

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

  std::cout << "Requested SPI clock:    " << Ad9833::default_spi_config().spi_clock_hz << " Hz\n";
  std::cout << "Achieved  SPI clock:    " << dds.achieved_spi_clock_hz() << " Hz\n";
  std::cout << "Half-period ticks:      " << dds.half_period_ticks() << "\n";
  std::cout << "AD9833 master clock:    " << dds.master_clock_hz() << " Hz\n";
  std::cout << "Requested output freq:  " << kTargetOutputFrequencyHz << " Hz\n";
  std::cout << "Realized  output freq:  " << std::fixed << std::setprecision(9) << realized_output_frequency_hz << " Hz\n";
  print_word32(freq0_tuning_word, "round(f_out * 2^28 / MCLK) for 5 MHz on a 25 MHz AD9833 clock");
  print_word(control_reset, "control(B28 | RESET)");
  print_word(freq0_words[0], "FREQ0 low14(tuning_word)");
  print_word(freq0_words[1], "FREQ0 high14(tuning_word)");
  print_word(phase0_zero, "PHASE0(0)");
  print_word(control_run, "control(B28, waveform=sine, FREQ0, PHASE0)");

  std::cout << "Programming words:      ";
  for (const uint16_t word : {control_reset, freq0_words[0], freq0_words[1], phase0_zero, control_run})
    std::cout << "0x" << std::hex << word << " ";
  std::cout << std::dec << "\n\n";

  spi_payload::dump_tokens(dds.tokens());

  std::cout << "qout[19]=FSY qout[18]=CLK qout[17]=DAT qout[16]=CS(MCP41010)" << std::endl;

  std::ofstream sequence_file("sequence");
  spi_payload::dump_tokens_seq(dds.tokens(), sequence_file);
  return 0;
}
