// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Streaming using the direct memory access

#pragma once

#include <iostream>
#include <thread>
#include <algorithm>
#include <unistd.h>

#include "basic_multi_dma.hh"
#include "readback.hh"
#include "parser.hh"
#include "streamer.hh"
#include "config.h"
#include "definitions.hh"
#include "ppworkflow.hh"

class dmatests {
 public:
   dma_streamer &ds;
   readback &rb;
   counter &ctr;
   const InputParser &input;
   const Verbosity &verb;

   static void trig_force(streamer_control &sc) {
     const int delay = 100*1000;
     usleep(delay);
     sc.trigger_force();
     std::cout << "%%% Triggered." << std::endl;
   }

   int test4() {
     std::cout << "test4 - sequential counter (or randomized values using the -rnd switch) with random duration" << std::endl;
     const auto c = parse_count(input, "-c", "10000");
     const auto vmax = parse_value(input, "-v", "10");
     const auto rnd = input.exists("-rnd");
     auto elements = prepare_random_test_sequence(vmax, c, rnd);
     return send_and_trig(ds.dma, ds.sc, rb, ctr, elements, input, force_trigger, verb);
   }

   size_t write_sequence(bool terminator) {
     const auto c = parse_count(input, "-c", "1000");
     auto v = parse_value(input, "-v", "10");
     const auto vmax = ds.dma.max_size/BYTES_TOTAL-1; // maximum number of elements (include one position for terminal element)
     v = std::min(v, vmax);
     size_t len = v;
     if (verb.verbose)
       std::cout << "c=" << std::dec << c << " v=" << v << std::endl;
     for (size_t i = 0; i < v; i++)
       ds.dma.write_element(i, el(c, i));
     if (terminator) {
       ds.dma.write_element(v, el());
       len++;
     }
     return len;
   }

   int test21() {
     std::cout << "test21 - long sequence (DMA)" << std::endl;
     const auto len = write_sequence(true);
     std::thread trig(trig_force, std::ref(ds.sc));
     ds.dma.transfer(BYTES_TOTAL*len);
     trig.join();
     return ds.sc.wait_to_complete(verb);
   }

   int test22() {
     std::cout << "test22 - loops of long sequence (DMA)" << std::endl;
     const auto len = write_sequence(false);
     const auto reps = parse_count(input, "-reps", "0"); // repetitions, 0 = infinity
     std::cout << "reps=" << std::dec << reps << std::endl;
     std::thread trig(trig_force, std::ref(ds.sc));
     ds.dma.transfer_multiple_times(BYTES_TOTAL*len, reps);
     trig.join();
     return ds.sc.wait_to_complete(verb);
   }

   dmatests(dma_streamer &_ds, readback &_rb, counter &_ctr, const InputParser &_input, const Verbosity &_v) :
     ds(_ds), rb(_rb), ctr(_ctr), input(_input), verb(_v) {}

   int run(const int test) {
     std::cout << "Requested test " << std::dec << test << std::endl;
     int rc = 0;
     switch (test) {
     case 0:
       std::cout << "There is nothing to do!" << std::endl;
       break;
     case 4:
       rc = test4();
       break;
     case 21:
       rc = test21();
       break;
     case 22:
       rc = test22();
       break;
     default:
       std::cerr << "Unknown test " << test << std::endl;
       break;
     }
     return rc;
   }
};
