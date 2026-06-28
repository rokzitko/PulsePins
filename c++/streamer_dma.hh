// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// DMA-backed sequence transport for the PulsePins streamer.
//
// This transport writes the sequence into SDRAM first, then asks the MSGDMA engine to feed
// that buffer into the streamer ingress path. It is useful when long sequences or repeated
// replays would make CPU-driven FIFO writes less attractive.

#pragma once

#include <cstdint> // integer types, uint32_t, etc.
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h> // usleep

#include "tidbit.hh"
#include "address_map.hh"
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

static_assert(address_map::contains(address_map::h2f::msgdma_1_csr, 0x08));
static_assert(address_map::contains(address_map::h2f::msgdma_1_descriptor_slave, 0x0C));

// DMA transport wrapper for one streamer input path.
class streamer_dma : private c_dma
{
public:
  size_t max_size; // in bytes
  mm sdram;
  int report_interval = 300; // report DMA status every report_interval milliseconds when waiting for transfer to complete
  const Verbosity &v;

  void enqueue_sdram_range(const size_t offset, const size_t size) {
    if (size == 0)
      throw std::invalid_argument("DMA transfer size must be nonzero");
    if (offset > max_size || size > max_size - offset)
      throw std::runtime_error("DMA transfer exceeds configured staging buffer size.");
    if (offset > UINTPTR_MAX - sdram.get_base())
      throw std::out_of_range("DMA transfer address overflows uintptr_t");

    enqueue_src_addr(sdram.get_base() + offset, checked_descriptor_u32(size, "length"));
  }

  streamer_dma(const mm &dev,
                const address_map::H2fRegion csr_base,
                const address_map::H2fRegion descriptor_base,
                const std::uintptr_t ram_addr,
                const size_t _max_size,
                const Verbosity &_v) :
    c_dma(dev, csr_base.base, descriptor_base.base, _v.veryverbose),
    max_size(_max_size),
    sdram(ram_addr, max_size),
    v(_v) {
      reset();
    }

  // Materialize one element into the SDRAM staging buffer at logical position `i`.
  void write_element(const int i, const el &e) {
    if (i < 0)
      throw std::out_of_range("DMA buffer element index must be non-negative");
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
    if (size > max_size)
      throw std::runtime_error("DMA buffer size exceeds configured staging buffer size.");
    return size; // return the size of the data in bytes
  }

  template <typename T>
  [[noreturn]] void throw_verify_mismatch(const size_t element_index,
                                          const char *field,
                                          const size_t byte_offset,
                                          const T expected,
                                          const T actual) const {
    std::ostringstream ss;
    ss << "DMA buffer mismatch at element " << std::dec << element_index
       << " field " << field
       << " byte_offset=" << byte_offset
       << ": expected=0x" << std::hex << static_cast<uint64_t>(expected)
       << " actual=0x" << static_cast<uint64_t>(actual);
    throw std::runtime_error(ss.str());
  }

  // Re-read the SDRAM staging buffer and check that it matches the source sequence.
  bool verify(const Sequence &elements) {
    size_t pos = 0; // in units of words (32-bits, 4-bytes)
    size_t element_index = 0;
    for(const auto &e : elements) {
      if (4*pos + BYTES_TOTAL <= max_size) { // still fits in buffer
        const size_t control_offset = pos*4;
        auto py = sdram.get_ptr(pos*4);
        const auto actual_control = *(control_t*)py;
        if (actual_control != e.control())
          throw_verify_mismatch(element_index, "control", control_offset, e.control(), actual_control);
        pos++;
        const size_t count_offset = pos*4;
        auto pc = sdram.get_ptr(pos*4);
        const auto actual_count = *(count_t*)pc;
        if (actual_count != e.count())
          throw_verify_mismatch(element_index, "count", count_offset, e.count(), actual_count);
        pos++;
        const size_t value_offset = pos*4;
        auto pv = sdram.get_ptr(pos*4);
        const auto actual_value = *(value_t*)pv;
        if (actual_value != e.value())
          throw_verify_mismatch(element_index, "value", value_offset, e.value(), actual_value);
        pos++;
      } else {
        throw std::runtime_error("DMA buffer verification exceeds configured staging buffer size.");
      }
      element_index++;
    }
    if (4 * pos > max_size)
      throw std::runtime_error("DMA buffer verification exceeds configured staging buffer size.");
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
  void transfer_multiple_times(
    const size_t size,
    const size_t repetitions,
    const std::optional<size_t> final_offset = std::nullopt,
    const size_t final_size = 0) {
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
    if (final_offset.has_value()) {
      enqueue_sdram_range(*final_offset, final_size);
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
    if (verify_buffer)
      verify(elements);
    transfer(size);
  }
};
