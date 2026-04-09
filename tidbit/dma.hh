// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Direct-memory access

#pragma once

// https://www.intel.com/content/www/us/en/docs/programmable/683130/21-4/modular-scatter-gather-dma-core.html

#include <iostream>
#include <bitset>
#include <string>

#include "tidbit.hh"

class c_dma {
protected:
   loc dma_csr_status, dma_csr_control, dma_csr_fill_level; // control
   loc d_src_addr, d_dest_addr, d_length, d_control; // descriptors
   const bool verbose;

public:
   c_dma(const mm &dev,
         const std::uintptr_t csr_base,
         const std::uintptr_t descriptor_base,
         const bool _verbose = true,
         std::string name = "dma"s) :
     dma_csr_status(dev.get_addr(csr_base),           name + "/csr_status"),
     dma_csr_control(dev.get_addr(csr_base, 0x04),    name + "/csr_control"),
     dma_csr_fill_level(dev.get_addr(csr_base, 0x08), name + "/csr_fill_level"),
     d_src_addr(dev.get_addr(descriptor_base, 0x00),  name + "/src_addr"),
     d_dest_addr(dev.get_addr(descriptor_base, 0x04), name + "/dest_addr"),
     d_length(dev.get_addr(descriptor_base, 0x08),    name + "/length"),
     d_control(dev.get_addr(descriptor_base, 0x0C),   name + "/control"),
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
     const uint16_t write_fill = (uint16_t)(fill >> 16);
     const uint16_t read_fill = (uint16_t)(fill & 0x0000ffffUL);

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

   uint16_t write_fill() {
     const uint32_t fill = dma_csr_fill_level.read();
     const uint16_t write_fill = (uint16_t)(fill >> 16);
     return write_fill;
   }

   uint16_t read_fill() {
     const uint32_t fill = dma_csr_fill_level.read();
     const uint16_t read_fill = (uint16_t)(fill & 0x0000ffffUL);
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
      dma_csr_control.write(1 << 0); // stop dispatcher (STOP_DISPATCHER)
      dma_csr_control.write(1 << 1); // reset dispatcher (RESET_DISPATCHER)
      dma_csr_control.write(0);
      uint32_t s;
      while ((s=dma_csr_status.read()) & (1 << 6)) { usleep(1000); } // (RESETTING)
      dma_csr_status.write(s & ~(1UL << 6)); // clear reset bit
      status();
      dma_csr_control.write(1 << 5); // stop descriptors (STOP_DESCR)
      clear_status();
      status();
   }

   void reset2() {
      status();
      control_setbit(1UL << 0); // stop dispatcher (STOP_DISPATCHER)
      control_clearbit(1UL << 0);
      control_setbit(1UL << 1); // reset dispatcher (RESET_DISPATCHER)
      control_clearbit(1UL << 1);
      status();
      while (dma_csr_status.read() & (1 << 6)) { usleep(1000); } // (RESETTING)
//      control_setbit((1UL << 3) | (1UL << 2) | (1UL << 4)); // (STOP_ON_EARLY_TERM), (STOP_ON_ERROR), (GLOBAL_INT_EN_MASK)
      control_setbit((1UL << 3) | (1UL << 2)); // (STOP_ON_EARLY_TERM), (STOP_ON_ERROR), (GLOBAL_INT_EN_MASK)
      status();
   }

   // wait until the DMA transfer queue is empty
   void wait(int sleep_us = 100*1000) {
     uint32_t s = status();
     size_t cnt = 0;
     while (s & (1<<0) ) { // wait until busy flag is cleared
       usleep(sleep_us);
       std::cout << "#" << cnt << " ";
       s = status();
       cnt++;
     }
   }

   void initiate_transfer() {
     if (verbose) std::cout << "Initiating transfer." << std::endl;
     status();
     const uint32_t control = dma_csr_control.read();
     dma_csr_control.write((control | (1UL<<2)) & ~(1UL<<5) & ~(1UL<<4)); // + stop on errors, - stop_descriptors, - global_interrupt,
   }

   void enqueue(const uintmax_t src_addr, const uintmax_t dest_addr, const uint32_t length, const uint32_t control = 0UL) {
      if (verbose) {
        printf("physical src=0x%jx dest=0x%jx\n", (uintmax_t)src_addr, (uintmax_t)dest_addr);
        std::cout << "length(bytes)=" << std::dec << length << " 0x" << std::hex << length
          << " control=" << control << std::endl;
      }
      d_src_addr.write(src_addr);
      d_dest_addr.write(dest_addr);
      d_length.write(length);
      d_control.write(control | (1ULL << 31));
   }

   void enqueue_src_addr(const uintmax_t src_addr, const uint32_t length, const uint32_t control = 0UL) {
     if (verbose) {
       printf("physical src=0x%jx\n", (uintmax_t)src_addr);
       std::cout << "length(bytes)=" << std::dec << length << " 0x" << std::hex << length
         << " control=" << control << std::endl;
     }
     d_src_addr.write(src_addr);
     d_length.write(length);
     d_control.write(control | (1ULL << 31));
   }

   void enqueue_dest_addr(const uintmax_t dest_addr, const uint32_t length, const uint32_t control = 0UL) {
     if (verbose) {
       printf("physical dest=0x%jx\n", (uintmax_t)dest_addr);
       std::cout << "length(bytes)=" << std::dec << length << " 0x" << std::hex << length
         << " control=" << control << std::endl;
     }
     d_dest_addr.write(dest_addr);
     d_length.write(length);
     d_control.write(control | (1ULL << 31));
   }

   void enqueue_dest_addr(const ram_block &rb) {
     enqueue_dest_addr(rb.get_addr(), rb.get_size());
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
     reset();
     for (int i = 0; i < chunks; i++)
       enqueue(0x0, ram_addr+i*size/chunks, size/chunks); // source addr irrelevant
     if (verbose) std::cout << "Transfering." << std::endl;
     initiate_transfer();
   }
};
