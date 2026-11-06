#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "ad9959.hh"

using spi_payload::Ad9959;

namespace {

void usage(std::ostream &out)
{
  out << "Usage: spi_ad9959 [output.seq [CH0_Hz CH1_Hz CH2_Hz CH3_Hz]]\n"
      << "Defaults: ad9959.seq, 50e6 75e6 100e6 125e6\n"
      << "Requires a 100 MHz PulsePins streamer and 25 MHz DDS reference (PLL x20).\n";
}

double parse_frequency(const char *argument)
{
  const std::string text(argument);
  std::size_t consumed = 0;
  const double frequency = std::stod(text, &consumed);
  if (consumed != text.size())
    throw std::invalid_argument("Frequency must be a number in Hz: " + text);
  return frequency;
}

} // namespace

int main(int argc, char **argv)
{
  if (argc == 2 && std::string(argv[1]) == "--help") {
    usage(std::cout);
    return 0;
  }
  if (argc != 1 && argc != 2 && argc != 6) {
    usage(std::cerr);
    return 1;
  }

  try {
    const std::string filename = argc >= 2 ? argv[1] : "ad9959.seq";
    auto frequencies = Ad9959::kDefaultFrequenciesHz;
    if (argc == 6)
      for (std::size_t channel = 0; channel < frequencies.size(); ++channel)
        frequencies[channel] = parse_frequency(argv[channel + 2]);
    const auto tokens = Ad9959::tone_sequence(frequencies);

    std::ofstream file;
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.open(filename);
    uint64_t ticks = 0;
    for (const auto &token : tokens) {
      file << "d " << std::dec << token.repeat << " 0x" << std::hex << token.pins << "\n";
      ticks += token.repeat;
    }
    file << "final 0x2\nf\n";
    file.close();

    std::cout << "DDS reference: 25 MHz; PLL x20; system clock: 500 MHz\n"
              << "PulsePins streamer: 100 MHz; SPI: 12.5 MHz, mode 0, 1-bit MSB-first\n";
    for (std::size_t channel = 0; channel < frequencies.size(); ++channel) {
      const auto word = Ad9959::tuning_word_for_frequency(frequencies[channel]);
      std::cout << "CH" << channel << ": " << std::fixed << std::setprecision(6)
                << frequencies[channel] << " Hz, FTW=0x" << std::hex
                << std::setw(8) << std::setfill('0') << word << std::dec
                << std::setfill(' ') << ", realized=" << std::setprecision(9)
                << Ad9959::realized_frequency_hz(word) << " Hz\n";
    }
    std::cout << "Wrote " << filename << ": " << tokens.size() << " data records, "
              << ticks << " ticks (" << ticks / 100.0 << " us), final qout=0x2\n";
    return 0;
  }
  catch (const std::exception &error) {
    std::cerr << "spi_ad9959: " << error.what() << "\n";
    return 1;
  }
}
