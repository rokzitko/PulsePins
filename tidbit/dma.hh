// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Direct-memory access

#pragma once

// https://www.intel.com/content/www/us/en/docs/programmable/683130/21-4/modular-scatter-gather-dma-core.html

#include <iostream>
#include <bitset>
#include <stdexcept>
#include <string>
#include <limits>

#include "stall_timeout.hh"
#include "tidbit.hh"

class c_dma {
protected:
   static constexpr uint32_t dma_status_error_mask = (1U << 7) | (1U << 8);
   static constexpr uint32_t descriptor_control_go = uint32_t{1} << 31;

   loc dma_csr_status, dma_csr_control, dma_csr_fill_level; // control
   loc d_src_addr, d_dest_addr, d_length, d_control; // descriptors
   const bool verbose;

   void wait_for_reset_clear_or_timeout(const char *context,
                                        const double timeout_s = default_transport_stall_timeout_s) {
      TimeoutGuard watchdog(context, timeout_s);
      while (dma_csr_status.read() & (1 << 6)) {
        watchdog.throw_if_total_timeout(status_string());
        usleep(1000);
      }
    }

   static uint32_t checked_descriptor_u32(const uintmax_t value, const char *field) {
     if (value > std::numeric_limits<uint32_t>::max())
       throw std::out_of_range(std::string("DMA descriptor ") + field + " exceeds 32-bit register width");
     return static_cast<uint32_t>(value);
   }

public:
   c_dma(const mm &dev,
         const std::uintptr_t csr_base,
         const std::uintptr_t descriptor_base,
         const bool _verbose = true,
         std::string name = "dma"s) :
     dma_csr_status(dev, csr_base, name + "/csr_status"),
     dma_csr_control(dev, csr_base, 0x04, name + "/csr_control"),
     dma_csr_fill_level(dev, csr_base, 0x08, name + "/csr_fill_level"),
     d_src_addr(dev, descriptor_base, 0x00, name + "/src_addr"),
     d_dest_addr(dev, descriptor_base, 0x04, name + "/dest_addr"),
     d_length(dev, descriptor_base, 0x08, name + "/length"),
     d_control(dev, descriptor_base, 0x0C, name + "/control"),
     verbose(_verbose) {}

   std::string status_string() {
     const uint32_t status = dma_csr_status.read();
     std::string str;
     str =  ( status & (1ULL << 0) ? " busy" : "");
     str += ( status & (1ULL << 1) ? " descriptor_buffer_empty" : "");
     str += ( status & (1ULL << 2) ? " descriptor_buffer_full" : "");
     str += ( status & (1ULL << 3) ? " response_buffer_empty" : "");
     str += ( status & (1ULL << 4) ? " response_buffer_full" : "");
     str += ( status & (1ULL << 5) ? " stopped" : "");
     str += ( status & (1ULL << 6) ? " resetting" : "");
     str += ( status & (1ULL << 7) ? " stopped_on_error" : "");
     str += ( status & (1ULL << 8) ? " stopped_on_early_termination" : "");
     str += ( status & (1ULL << 9) ? " IRQ" : "");

     const uint32_t fill = dma_csr_fill_level.read();
     const uint16_t write_fill = static_cast<uint16_t>(fill >> 16);
     const uint16_t read_fill = static_cast<uint16_t>(fill & 0x0000ffffUL);

     const uint32_t control = dma_csr_control.read();
     std::string strc;
     strc  = (control & (1ULL << 0) ? " stop_dispatcher" : "");
     strc += (control & (1ULL << 1) ? " reset_dispatcher" : "");
     strc += (control & (1ULL << 2) ? " stop_on_error" : "");
     strc += (control & (1ULL << 3) ? " stop_on_early_term" : "");
     strc += (control & (1ULL << 4) ? " global_int_en_mask" : "");
     strc += (control & (1ULL << 5) ? " stop_desc" : "");

     std::stringstream ss;
     ss << "status=0x" << std::hex << status << str
       << " | control=0x" << std::hex << control << strc
       << " | fill r=" << std::dec << read_fill << " w=" << write_fill;
     return ss.str();
   }

   uint32_t status() {
     const uint32_t status = dma_csr_status.read();
     if (verbose)
       std::cout << status_string() << std::endl;
     return status;
   }

   void throw_if_error_status(const char *context) {
     const uint32_t s = dma_csr_status.read();
     if (s & dma_status_error_mask)
       throw std::runtime_error(std::string(context) + " failed: " + status_string());
   }

   uint16_t write_fill() {
     const uint32_t fill = dma_csr_fill_level.read();
     const uint16_t write_fill = static_cast<uint16_t>(fill >> 16);
     return write_fill;
   }

   uint16_t read_fill() {
     const uint32_t fill = dma_csr_fill_level.read();
     const uint16_t read_fill = static_cast<uint16_t>(fill & 0x0000ffffUL);
     return read_fill;
   }

   void clear_status() {
      const uint32_t status = dma_csr_status.read();
      dma_csr_status.write(status);
   }

   void control_setbit(const uint32_t mask) {
     const uint32_t val = dma_csr_control.read();
     dma_csr_control.write(val | mask);
   }

   void control_clearbit(const uint32_t mask) {
     const uint32_t val = dma_csr_control.read();
     dma_csr_control.write(val & (~mask));
   }

   void reset() {
      status();
      // reset msgdma
      dma_csr_control.write(uint32_t{1} << 0); // stop dispatcher (STOP_DISPATCHER)
      dma_csr_control.write(uint32_t{1} << 1); // reset dispatcher (RESET_DISPATCHER)
      dma_csr_control.write(0);
      uint32_t s;
      wait_for_reset_clear_or_timeout("DMA reset");
      s = dma_csr_status.read();
      dma_csr_status.write(s & ~(uint32_t{1} << 6)); // clear reset bit
      status();
      dma_csr_control.write(uint32_t{1} << 5); // stop descriptors (STOP_DESCR)
      clear_status();
      status();
   }

   void reset2() {
      status();
      control_setbit(uint32_t{1} << 0); // stop dispatcher (STOP_DISPATCHER)
      control_clearbit(uint32_t{1} << 0);
      control_setbit(uint32_t{1} << 1); // reset dispatcher (RESET_DISPATCHER)
      control_clearbit(uint32_t{1} << 1);
      status();
      wait_for_reset_clear_or_timeout("DMA reset2");
//      control_setbit((uint32_t{1} << 3) | (uint32_t{1} << 2) | (uint32_t{1} << 4)); // (STOP_ON_EARLY_TERM), (STOP_ON_ERROR), (GLOBAL_INT_EN_MASK)
      control_setbit((uint32_t{1} << 3) | (uint32_t{1} << 2)); // (STOP_ON_EARLY_TERM), (STOP_ON_ERROR), (GLOBAL_INT_EN_MASK)
      status();
   }

   // wait until the DMA transfer queue is empty
    void wait(int sleep_us = 100*1000, const double timeout_s = default_transport_busy_timeout_s) {
      if (sleep_us < 0)
        throw std::invalid_argument("DMA wait sleep interval must be nonnegative");
      TimeoutGuard watchdog("DMA busy wait", timeout_s);
      uint32_t s = status();
      size_t cnt = 0;
      while (s & (1<<0) ) { // wait until busy flag is cleared
        watchdog.throw_if_total_timeout(status_string());
        usleep(static_cast<useconds_t>(sleep_us));
        std::cout << "#" << cnt << " ";
        s = status();
       cnt++;
     }
     throw_if_error_status("DMA transfer");
   }

   void initiate_transfer() {
     if (verbose) std::cout << "Initiating transfer." << std::endl;
     status();
     const uint32_t control = dma_csr_control.read();
     dma_csr_control.write((control | (uint32_t{1} << 2)) & ~(uint32_t{1} << 5) & ~(uint32_t{1} << 4)); // + stop on errors, - stop_descriptors, - global_interrupt,
   }

   void enqueue(const uintmax_t src_addr, const uintmax_t dest_addr, const uint32_t length, const uint32_t control = 0UL) {
      if (verbose) {
        printf("physical src=0x%jx dest=0x%jx\n", src_addr, dest_addr);
        std::cout << "length(bytes)=" << std::dec << length << " 0x" << std::hex << length
          << " control=" << control << std::endl;
      }
      d_src_addr.write(checked_descriptor_u32(src_addr, "source address"));
      d_dest_addr.write(checked_descriptor_u32(dest_addr, "destination address"));
      d_length.write(length);
      d_control.write(control | descriptor_control_go);
   }

   void enqueue_src_addr(const uintmax_t src_addr, const uint32_t length, const uint32_t control = 0UL) {
     if (verbose) {
       printf("physical src=0x%jx\n", src_addr);
       std::cout << "length(bytes)=" << std::dec << length << " 0x" << std::hex << length
         << " control=" << control << std::endl;
     }
     d_src_addr.write(checked_descriptor_u32(src_addr, "source address"));
     d_length.write(length);
     d_control.write(control | descriptor_control_go);
   }

   void enqueue_dest_addr(const uintmax_t dest_addr, const uint32_t length, const uint32_t control = 0UL) {
     if (verbose) {
       printf("physical dest=0x%jx\n", dest_addr);
       std::cout << "length(bytes)=" << std::dec << length << " 0x" << std::hex << length
         << " control=" << control << std::endl;
     }
     d_dest_addr.write(checked_descriptor_u32(dest_addr, "destination address"));
     d_length.write(length);
     d_control.write(control | descriptor_control_go);
   }

   void enqueue_dest_addr(const ram_block &rb) {
     enqueue_dest_addr(rb.get_addr(), checked_descriptor_u32(rb.get_size(), "length"));
   }

   // Add to queue and immediately initiate writing.
   void send(const uintmax_t src_addr, const uintmax_t dest_addr, const uint32_t length) {
      reset();
      enqueue(src_addr, dest_addr, length);
      initiate_transfer();
      wait();
   }

   void send(const uintmax_t paddr, const uint32_t length) {
     send(paddr, 0, length);
   }

   // High-level functions
   void read_in_chunks(std::uintptr_t ram_addr, const size_t size, const int chunks) {
     if (chunks <= 0)
       throw std::invalid_argument("DMA chunk count must be positive");
     const size_t chunk_count = static_cast<size_t>(chunks);
     const size_t chunk_size = size/chunk_count;
     const size_t remainder = size % chunk_count;
     if (chunk_size == 0)
       throw std::invalid_argument("DMA chunk size must be nonzero");
     reset();
     for (size_t i = 0; i < chunk_count; i++) {
       if (i > (UINTPTR_MAX - ram_addr)/chunk_size)
         throw std::out_of_range("DMA chunk address overflows uintptr_t");
       const size_t this_chunk_size = chunk_size + (i + 1 == chunk_count ? remainder : 0);
       const uint32_t chunk_length = checked_descriptor_u32(this_chunk_size, "length");
       enqueue(0x0, ram_addr + i*chunk_size, chunk_length); // source addr irrelevant
     }
     if (verbose) std::cout << "Transfering." << std::endl;
     initiate_transfer();
   }
};
