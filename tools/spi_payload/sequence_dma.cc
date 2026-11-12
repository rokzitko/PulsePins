// Finite, CRC-checked DMA playback using the headers installed on the bench board.
// This helper is device-independent; the Python caller supplies the pin waveform.
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <thread>

#include "host_runtime.hh"
#include "basic_multi_dma.hh"
#include "ppversion.hh"
#include "readback.hh"
#include "trigger.hh"

bool exit_flag = false;

int main(int argc, char *argv[])
{
  if (argc == 2 && std::string(argv[1]) == "--help") {
    std::cout << "sequence_dma -file FILE -crc EXPECTED_CRC -i INITIAL_PINS\n"
                 "Finite d/store/r/final/f sequences only; at most 5 seconds at 100 MHz.\n"
                 "Checks output and readback CRC, completion, FIFO accounting and final pins.\n"
                 "Raw readback FIFO overflow is reported separately from the full-stream CRC.\n";
    return 0;
  }
  try {
    // Parse and validate before constructing hardware-owning objects.
    InputParser arguments(argc, argv);
    if (!arguments.exists("-crc") || !arguments.exists("-i"))
      throw std::runtime_error("Both -crc and -i are required");
    const auto number = [](const std::string &text) {
      std::size_t used = 0;
      const auto value = std::stoull(text, &used, 0);
      if (used != text.size() || text.front() == '-')
        throw std::runtime_error("Invalid unsigned integer: " + text);
      return value;
    };
    std::ifstream file(arguments.get_string("-file", ""));
    if (!file)
      throw std::runtime_error("Cannot open sequence file");
    Sequence sequence;
    std::array<uint32_t, 8> slot_ticks{};
    std::string op;
    uint64_t ticks = 0;
    value_t final = 0;
    bool terminated = false;
    while (file >> op) {
      if (op == "f")
        continue;
      if (terminated)
        throw std::runtime_error("Record after final");
      std::string count, value;
      if (op == "d" && file >> count >> value) {
        const auto n = number(count);
        const auto pins = number(value);
        if (!n || n > UINT32_MAX || pins > UINT32_MAX)
          throw std::runtime_error("Invalid data record");
        sequence.push_back(el(static_cast<count_t>(n), static_cast<value_t>(pins)));
        ticks += n;
      } else if (op == "store") {
        std::string slot, regular;
        if (!(file >> slot >> regular >> count >> value) || regular != "d")
          throw std::runtime_error("Expected store SLOT d COUNT PINS");
        const auto index = number(slot);
        const auto n = number(count);
        const auto pins = number(value);
        if (index >= slot_ticks.size() || !n || n > UINT32_MAX || pins > UINT32_MAX)
          throw std::runtime_error("Invalid stored data record");
        slot_ticks[index] = static_cast<uint32_t>(n);
        sequence.push_back(el(static_cast<count_t>(n), static_cast<value_t>(pins)).stored_in(index));
      } else if (op == "r" && file >> count >> value) {
        const auto repetitions = number(count);
        const auto length = number(value);
        if (!repetitions || repetitions > UINT32_MAX || !length || length > slot_ticks.size())
          throw std::runtime_error("Expected finite replay of 1..8 slots");
        uint64_t period = 0;
        for (std::size_t index = 0; index < length; ++index) {
          if (!slot_ticks[index])
            throw std::runtime_error("Replay references an uninitialized slot");
          period += slot_ticks[index];
        }
        if (period > 500000000 / repetitions)
          throw std::runtime_error("Replay exceeds five-second limit");
        ticks += period * repetitions;
        sequence.push_back(el(Replay{}, static_cast<count_t>(repetitions), static_cast<value_t>(length)));
      } else if (op == "final" && file >> value) {
        const auto pins = number(value);
        if (pins > UINT32_MAX)
          throw std::runtime_error("Invalid final pins");
        final = static_cast<value_t>(pins);
        sequence.push_back(el(final));
        terminated = true;
      } else {
        throw std::runtime_error("Expected d/store/r/final/f sequence");
      }
      if (ticks > 500000000)
        throw std::runtime_error("Sequence exceeds five-second limit");
    }
    if (!terminated || !ticks || ticks > 500000000 || sequence.size() > dma_size / BYTES_TOTAL)
      throw std::runtime_error("Missing final, empty sequence, or sequence too large/long");

    HostRuntime runtime(argc, argv, version);
    auto &fpga = runtime.get_fpga();
    const auto expected_crc = parse_value(runtime.input, "-crc", "0");
    runtime.input.get_string("-file", "");
    if (std::abs(fpga.streamer_freq() - 100e6) > 1000)
      throw std::runtime_error("Expected 100 MHz streamer");
    dma_streamer stream(runtime.input, fpga);
    trigger trig(runtime.input, fpga);
    readback capture(runtime.input, fpga);
    const auto bytes = stream.dma.prepare(sequence);
    stream.dma.verify(sequence);
    stream.dma.report_interval = 1;
    std::atomic<bool> transfer_finished{false};
    std::exception_ptr transfer_error;
    std::thread transfer([&] {
      try {
        stream.dma.transfer(bytes);
      } catch (...) {
        transfer_error = std::current_exception();
      }
      transfer_finished = true;
    });
    try {
      // Trigger on observed FIFO priming, never on a guessed host delay.
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      const auto primed = std::min<uint64_t>(ticks, SIZE_FIFO_OUT - 64);
      while (stream.sc.get_output_fifo_ctr_in() < primed) {
        if (transfer_finished && transfer_error)
          std::rethrow_exception(transfer_error);
        if (std::chrono::steady_clock::now() >= deadline)
          throw std::runtime_error("DMA did not prime the output FIFO");
        usleep(100);
      }
      stream.sc.trigger_force();
      transfer.join();
      if (transfer_error)
        std::rethrow_exception(transfer_error);
      if (stream.sc.wait_to_complete(runtime.verbosity) != RC_OK)
        throw std::runtime_error("Streamer completion/underrun failure");
      usleep(1000); // allow hardware status snapshots to cross clock domains
      const auto output_crc = stream.sc.get_crc32();
      const auto readback_crc = capture.get_crc32();
      const bool valid = stream.sc.done() && !stream.sc.buffer_error() && !stream.sc.get_overflow()
          && stream.sc.get_qout() == final && output_crc == expected_crc && readback_crc == expected_crc
          && stream.sc.get_input_fifo1_ctr_in() == sequence.size()
          && stream.sc.get_output_fifo_ctr_out() == ticks + 1
          && stream.sc.get_input_fifo1_ctr_in() == stream.sc.get_input_fifo1_ctr_out()
          && stream.sc.get_input_fifo2_ctr_in() == stream.sc.get_input_fifo2_ctr_out()
          && stream.sc.get_output_fifo_ctr_in() == stream.sc.get_output_fifo_ctr_out();
      stream.sc.statistics();
      capture.status_report();
      std::cout << "DMA_RESULT valid=" << valid << " expected_crc=0x" << std::hex << expected_crc
                << " output_crc=0x" << output_crc << " readback_crc=0x" << readback_crc
                << " final=0x" << stream.sc.get_qout() << std::dec
                << " ticks=" << ticks << " capture_overflow=" << capture.overflow() << std::endl;
      stream.sc.trigger_clear();
      return valid ? 0 : RC_ERROR_CHECK;
    } catch (...) {
      std::cout << "DMA_FAILURE done=" << stream.sc.done()
                << " buffer_error=" << stream.sc.buffer_error() << std::endl;
      stream.sc.status_report();
      stream.sc.statistics();
      capture.status_report();
      stream.sc.stop(true);
      stream.sc.reset_streamer();
      stream.sc.trigger_clear();
      if (transfer.joinable())
        transfer.join();
      throw;
    }
  } catch (const std::exception &error) {
    std::cerr << "sequence_dma: " << error.what() << std::endl;
    return 1;
  }
}
