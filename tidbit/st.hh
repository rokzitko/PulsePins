// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#pragma once

class st_read
{
 protected:
   loc lcontrol;
   loc lstatus;
 public:
   void reset() {
     lcontrol.write(1+8); // assert reset_all + reset_fifo
     lcontrol.write(0);
   }

   st_read(const mm &dev,
           const std::uintptr_t base,
           const bool _reset = false) :
     lcontrol(dev.get_loc(base)),
     lstatus(dev.get_loc(base))
     {
       if (_reset) reset();
     }

   void reset_fifo() {
     lcontrol.write(1); // assert reset_fifo
     lcontrol.write(0);
   }

   void start() {
     lcontrol.write(4);
     lcontrol.write(0);
   }

   void stop() {
     lcontrol.write(2);
     lcontrol.write(0);
   }

   auto status() {
     auto val = lstatus.read();
     return val & 0x07; // data_start, data_stop, data_en
   }

   bool error() {
     auto val = lstatus.read(4);
     return val & 0x01;
   }

   bool running() {
     auto val = lstatus.read(8);
     return val & 0x01;
   }
};

class st_write
{
 protected:
   loc lcontrol;
   loc lstatus;
 public:
   st_write(const mm &dev,
            const std::uintptr_t base) :
     lcontrol(dev.get_loc(base)),
     lstatus(dev.get_loc(base)) {}

   void reset_fifo() {
     lcontrol.write(1); // assert reset_fifo
     lcontrol.write(0);
   }

   void reset() {
     lcontrol.write(1+8); // assert reset_fifo + reset_all
     lcontrol.write(0);
   }

   void start() {
     lcontrol.write(4); // assert start_running
     // do not deassert!
   }

   void stop() {
     lcontrol.write(2); // assert stop_running
     // do not deassert!
   }

   auto status() {
     auto val = lstatus.read();
     return val & 0x07; // data_start, data_stop, data_en
   }

   bool error() {
     auto val = lstatus.read(4);
     return val & 0x01;
   }

   bool running() {
     auto val = lstatus.read(8);
     return val & 0x01;
   }

   auto check() {
     auto val = lstatus.read(12);
     return val & 0x0f; // reset_all, start_running, stop_running, reset_fifo
   }
};
