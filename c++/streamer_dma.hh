// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// DMA-backed sequence transport for the PulsePins streamer.
//
// This transport writes the sequence into SDRAM first, then asks the MSGDMA engine to feed
// that buffer into the streamer ingress path. It is useful when long sequences or repeated
// replays would make CPU-driven FIFO writes less attractive.

#pragma once

#include <cassert>
#include <cstdint> // integer types, uint32_t, etc.
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unistd.h> // usleep

#include "tidbit.hh"
#include "delay.hh"
#include "fifo.hh"
#include "dma.hh"
#include "misc.hh"
#include "verbosity.hh"
#include "config.h"
#include "definitions.hh"
#include "colors.hh"
#include "elements.hh"
#include "sequence.hh"

// DMA transport wrapper for one streamer input path.
class streamer_dma : private c_dma
{
public:
  size_t max_size; // in bytes
  mm sdram;
  int report_interval = 300; // report DMA status every report_interval milliseconds when waiting for transfer to complete
  const Verbosity &v;

  streamer_dma(const mm &dev,
                const std::uintptr_t csr_base,
                const std::uintptr_t descriptor_base,
                const std::uintptr_t ram_addr,
                const size_t _max_size,
                const Verbosity &_v) :
    c_dma(dev, csr_base, descriptor_base, _v.veryverbose),
    max_size(_max_size),
    sdram(ram_addr, max_size),
    v(_v) {
      reset();
    }

  // Materialize one element into the SDRAM staging buffer at logical position `i`.
  void write_element(const int i, const el &e) {
    size_t pos = (BYTES_TOTAL/4)*i; // in units of words (32-bits, 4-bytes); BYTES_TOTAL is the total size of an element in bytes
    if (4*pos + BYTES_TOTAL <= max_size) { // still fits in buffer
      auto py = sdram.get_ptr(pos*4);
      *(control_t*)py = e.control();
      pos++;
      auto pc = sdram.get_ptr(pos*4);
      *(count_t*)pc = e.count();
      pos++;
      auto pv = sdram.get_ptr(pos*4);
      *(value_t*)pv = e.value();
      pos++;
    } else {
      throw std::runtime_error("Out of bounds.");
    }
  }

  // Copy the whole sequence into the SDRAM staging buffer and return the byte count.
  size_t prepare(const Sequence &elements) {
    size_t i = 0;
    for(const auto &e : elements)
      write_element(i++, e);
    const size_t size = BYTES_TOTAL*i;
    assert(size <= max_size);
    return size; // return the size of the data in bytes
  }

  // Re-read the SDRAM staging buffer and check that it matches the source sequence.
  bool verify(const Sequence &elements) {
    size_t pos = 0; // in units of words (32-bits, 4-bytes)
    for(const auto &e : elements) {
      if (4*pos + BYTES_TOTAL <= max_size) { // still fits in buffer
        auto py = sdram.get_ptr(pos*4);
        if (*(control_t*)py != e.control()) return false;
        pos++;
        auto pc = sdram.get_ptr(pos*4);
        if (*(count_t*)pc != e.count()) return false;
        pos++;
        auto pv = sdram.get_ptr(pos*4);
        if (*(value_t*)pv != e.value()) return false;
        pos++;
      }
    }
      assert(4 * pos <= max_size);
      return true;
    }

  void report() {
    std::cout << "DMA " << status_string() << std::endl;
  }

  // Submit one prepared buffer to the DMA engine and wait for completion.
  void transfer(const size_t size) {
    reset();
    enqueue_src_addr(sdram.get_base(), size);
    initiate_transfer();
    wait(report_interval*1000);
    if (v.veryverbose) report();
  }

  // Queue repeated transfers of the same prepared buffer.
  // `repetitions = 0` means repeat indefinitely.
  void transfer_multiple_times(const size_t size, const size_t repetitions) {
    constexpr size_t depth = MSGDMA_1_DESCRIPTOR_SLAVE_DESCRIPTOR_FIFO_DEPTH;  // Descriptor FIFO depth = 32
    reset();
    TimeoutGuard watchdog("DMA descriptor FIFO space wait", default_transport_stall_timeout_s);
    std::optional<uint16_t> previous_fill;
    for (size_t cnt = 0; cnt < repetitions || repetitions == 0; cnt++) {
      if (v.veryverbose) std::cout << "DMA transfer equeuing, cnt=" << std::dec << cnt << " fill=" << read_fill() << std::endl;
      if (v.veryverbose) report();
      enqueue_src_addr(sdram.get_base(), size);
      if (cnt == depth-4) {
        initiate_transfer();
        if (v.verbose) std::cout << "DMA transfer initiated." << std::endl;
      }
      while (1) {
        const auto fill = read_fill();
        if (fill < depth-2) {
          watchdog.progress();
          break;
        }
        watchdog.progress_if_changed(previous_fill, fill);
        watchdog.throw_if_stalled(status_string());
        usleep(100);
      }
    }
    initiate_transfer(); // if not done in the for loop
    wait(report_interval*1000);
    if (v.veryverbose) report();
  }

  // End-to-end helper: prepare, optionally verify, then transfer.
  void send_sequence(const Sequence &elements, const bool verify_buffer = true) {
    const size_t size = prepare(elements);
    if (verify_buffer && verify(elements) == false)
      throw std::runtime_error("DMA buffer mismatch.");
    transfer(size);
  }
};
