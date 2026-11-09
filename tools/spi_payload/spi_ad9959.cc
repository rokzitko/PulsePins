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
      << "       spi_ad9959 --set-ch1 output.seq CH1_Hz amplitude_code\n"
      << "       spi_ad9959 --test1 [output.seq]\n"
      << "       spi_ad9959 --test2 [output.seq]\n"
      << "       spi_ad9959 --test3 [output.seq]\n"
      << "       spi_ad9959 --test4 [output.seq]\n"
      << "       spi_ad9959 --test5 [output.seq]\n"
      << "       spi_ad9959 --test6 [output.seq]\n"
      << "       spi_ad9959 --test7 [output.seq]\n"
      << "       spi_ad9959 --test8 [output.seq]\n"
      << "       spi_ad9959 --test9 [output.seq]\n"
      << "       spi_ad9959 --test10 [output.seq]\n"
      << "Defaults: ad9959.seq, 50e6 75e6 100e6 125e6\n"
      << "--set-ch1: update initialized CH1 only; 0..200 MHz, amplitude code 0..1023, no reset.\n"
      << "Test 1: CH1 frequency staircase, 1 s per tone, six cycles; returns to 75 MHz.\n"
      << "Test 2: CH1 profile FSK at 70/80 MHz, 500 ms per tone, 24 cycles; returns to 75 MHz.\n"
      << "Test 3: CH1 16-level FSK, 60-90 MHz in scrambled order, 250 ms per tone, six cycles.\n"
      << "Test 4: CH1 hardware sweep 60-90 MHz, about 1 s up / 3 s down, six cycles.\n"
      << "Test 5: CH1 no-dwell sweep 60-90 MHz in about 500 ms, retriggered every 1 s, 24 cycles.\n"
      << "Test 6: CH1 amplitude staircase at 75 MHz, about 0/-6/-12/-18/-24 dB, 1 s per level, five cycles.\n"
      << "Test 7: CH1 sampled sinusoidal AM at 75 MHz, 1 kHz / 50% depth, eight samples, 24 s.\n"
      << "Test 8: CH1 0/180 degree PSK at 10 kHz, alternating 3 s PSK / 3 s plain carrier, four cycles.\n"
      << "Test 9: CH1 10 ms on/off bursts, abrupt / 0.5 ms ramped edges, alternating 5 s blocks for 20 s.\n"
      << "Test 10: CH1 DAC current versus digital scaling, full/half/quarter/eighth, 1 s each, three pairs.\n"
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
  const bool set_ch1 = argc >= 2 && std::string(argv[1]) == "--set-ch1";
  const int test = argc >= 2 && std::string(argv[1]) == "--test1" ? 1
                 : argc >= 2 && std::string(argv[1]) == "--test2" ? 2
                 : argc >= 2 && std::string(argv[1]) == "--test3" ? 3
                 : argc >= 2 && std::string(argv[1]) == "--test4" ? 4
                 : argc >= 2 && std::string(argv[1]) == "--test5" ? 5
                 : argc >= 2 && std::string(argv[1]) == "--test6" ? 6
                 : argc >= 2 && std::string(argv[1]) == "--test7" ? 7
                 : argc >= 2 && std::string(argv[1]) == "--test8" ? 8
                 : argc >= 2 && std::string(argv[1]) == "--test9" ? 9
                 : argc >= 2 && std::string(argv[1]) == "--test10" ? 10 : 0;
  if (set_ch1 ? argc != 5 : (test && argc != 2 && argc != 3) || (!test && argc != 1 && argc != 2 && argc != 6)) {
    usage(std::cerr);
    return 1;
  }

  try {
    const std::string filename = set_ch1 ? argv[2]
                               : test ? (argc == 3 ? argv[2] : "ad9959_test" + std::to_string(test) + ".seq")
                                     : (argc >= 2 ? argv[1] : "ad9959.seq");
    auto frequencies = Ad9959::kDefaultFrequenciesHz;
    uint32_t amplitude_code = 1023;
    if (set_ch1) {
      frequencies[1] = parse_frequency(argv[3]);
      const std::string text(argv[4]);
      std::size_t consumed = 0;
      const auto code = std::stoul(text, &consumed);
      if (consumed != text.size() || code > 1023)
        throw std::invalid_argument("Amplitude code must be an integer in 0..1023");
      amplitude_code = static_cast<uint32_t>(code);
    }
    if (argc == 6)
      for (std::size_t channel = 0; channel < frequencies.size(); ++channel)
        frequencies[channel] = parse_frequency(argv[channel + 2]);
    const bool replay_test = test == 7 || test == 8 || test == 9;
    const auto profile = test == 7 ? Ad9959::sinusoidal_am_sequence()
                       : test == 8 ? Ad9959::phase_keying_sequence()
                       : test == 9 ? Ad9959::burst_comparison_sequence() : Ad9959::ProfileSequence{};
    const auto tokens = set_ch1 ? Ad9959::channel1_sequence(frequencies[1], amplitude_code)
                      : test == 1 ? Ad9959::frequency_staircase_sequence()
                      : test == 2 ? Ad9959::two_level_fsk_sequence()
                      : test == 3 ? Ad9959::sixteen_level_fsk_sequence()
                      : test == 4 ? Ad9959::asymmetric_sweep_sequence()
                      : test == 5 ? Ad9959::no_dwell_sweep_sequence()
                      : test == 6 ? Ad9959::amplitude_staircase_sequence()
                      : test == 10 ? Ad9959::dac_current_comparison_sequence()
                      : replay_test ? profile.setup : Ad9959::tone_sequence(frequencies);

    std::ofstream file;
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.open(filename);
    uint64_t ticks = 0;
    std::size_t records = 0;
    const auto write_tokens = [&](const auto &part) {
      for (const auto &token : part) {
        file << "d " << std::dec << token.repeat << " 0x" << std::hex << token.pins << "\n";
        ticks += token.repeat;
        ++records;
      }
    };
    write_tokens(tokens);
    if (replay_test) {
      for (std::size_t slot = 0; slot < profile.period.size(); ++slot) {
        const auto &token = profile.period[slot];
        file << "store " << std::dec << slot << " d " << token.repeat
             << " 0x" << std::hex << token.pins << "\n";
        ticks += uint64_t(token.repeat) * profile.repetitions * profile.blocks;
        ++records;
      }
      for (unsigned block = 0; block < profile.blocks; ++block) {
        if (!profile.block_setup.empty())
          write_tokens(profile.block_setup.at(block));
        file << "r " << std::dec << profile.repetitions << " " << profile.period.size() << "\n";
        ++records;
        if (profile.idle_ticks) {
          file << "d " << std::dec << profile.idle_ticks << " 0x" << std::hex << Ad9959::kIdlePins << "\n";
          ticks += profile.idle_ticks;
          ++records;
        }
      }
      write_tokens(profile.restore);
    }
    file << "final 0x2\nf\n";
    file.close();

    std::cout << "DDS reference: 25 MHz; PLL x20; system clock: 500 MHz\n"
              << "PulsePins streamer: 100 MHz; SPI: 12.5 MHz, mode 0, 1-bit MSB-first\n";
    if (set_ch1)
      std::cout << "CH1-only update, no DDS reset; amplitude code " << amplitude_code << "/1023 of maximum.\n";
    if (test == 1)
      std::cout << "Test 1: CH1 cycles 50 -> 75 -> 100 -> 125 MHz, 1 s per tone, six cycles.\n"
                << "After about 24 s, returns to the following steady tones:\n";
    if (test == 2)
      std::cout << "Test 2: CH1 profile FSK 70 <-> 80 MHz, 500 ms per tone, 24 cycles.\n"
                << "After about 24 s, returns to the following steady tones:\n";
    if (test == 3)
      std::cout << "Test 3: CH1 hops over 60, 62, ..., 90 MHz, 250 ms per tone, six cycles.\n"
                << "Order (MHz): 60 74 64 86 70 80 62 90 76 66 84 72 88 68 82 78\n"
                << "After about 24 s, returns to the following steady tones:\n";
    if (test == 4)
      std::cout << "Test 4: CH1 sweeps 60 -> 90 -> 60 MHz, about 1 s up / 3 s down, six cycles.\n"
                << "RDW=516, FDW=172, step interval=2 us; endpoint times 0.998830 / 2.996490 s.\n"
                << "After about 24 s, returns to the following steady tones:\n";
    if (test == 5)
      std::cout << "Test 5: CH1 no-dwell sweeps 60 -> 90 MHz, then jumps to 60 MHz; 24 cycles.\n"
                << "RDW=1032, step interval=2 us; sweep time 0.499416 s, retriggered every 1 s.\n"
                << "After about 24 s, returns to the following steady tones:\n";
    if (test == 6)
      std::cout << "Test 6: CH1 stays at 75 MHz; amplitude scales 1023 -> 512 -> 256 -> 128 -> 64.\n"
                << "Levels about 0/-6/-12/-18/-24 dB relative to full output, 1 s each, five cycles.\n"
                << "After about 25 s, restores full amplitude and the following steady tones:\n";
    if (test == 7)
      std::cout << "Test 7: CH1 1 kHz AM, 50% depth; eight profile samples replayed for 24 s.\n"
                << "Expected first sidebands: carrier +/-1 kHz, about -12.3 dBc.\n"
                << "Uses store/replay: capture with ppplay -read and compare using check_readback.py.\n"
                << "After about 24 s, restores full amplitude and the following steady tones:\n";
    if (test == 8)
      std::cout << "Test 8: CH1 0/180 degree PSK, 50 us per phase, alternating 3 s PSK / 3 s plain carrier.\n"
                << "Four cycles; PSK suppresses the center carrier and creates lines at +/-10, +/-30, ... kHz.\n"
                << "Uses store/replay: capture with ppplay -read -quiet and check using check_readback.py --summary.\n"
                << "After about 24 s, returns to the following steady tones:\n";
    if (test == 9)
      std::cout << "Test 9: CH1 10 ms on/off bursts at 75 MHz; abrupt / ramped / abrupt / ramped, 5 s each.\n"
                << "Automatic ramps: ARR=61, 1 LSB steps, about 0.499 ms; short SPI pauses between blocks.\n"
                << "Capture with ppplay -read -quiet and check using check_readback.py --summary.\n"
                << "After about 20 s, restores full amplitude and the following steady tones:\n";
    if (test == 10)
      std::cout << "Test 10: CH1 stays at 75 MHz; full -> half -> quarter -> eighth, 1 s per level.\n"
                << "First 4 s: DAC current; next 4 s: digital scaling; repeat three times.\n"
                << "After about 24 s, restores full current/amplitude and the following steady tones:\n";
    for (std::size_t channel = 0; channel < frequencies.size(); ++channel) {
      if (set_ch1 && channel != 1)
        continue;
      const auto word = frequencies[channel] == 0.0 ? 0 : Ad9959::tuning_word_for_frequency(frequencies[channel]);
      std::cout << "CH" << channel << ": " << std::fixed << std::setprecision(6)
                << frequencies[channel] << " Hz, FTW=0x" << std::hex
                << std::setw(8) << std::setfill('0') << word << std::dec
                << std::setfill(' ') << ", realized=" << std::setprecision(9)
                << Ad9959::realized_frequency_hz(word) << " Hz\n";
    }
    std::cout << "Wrote " << filename << ": " << records << (replay_test ? " input records, " : " data records, ")
              << ticks << " ticks (" << ticks / 100.0 << " us), final qout=0x2\n";
    return 0;
  }
  catch (const std::exception &error) {
    std::cerr << "spi_ad9959: " << error.what() << "\n";
    return 1;
  }
}
