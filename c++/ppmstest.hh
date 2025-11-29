// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Multistreamer tests

#pragma once

#include <iostream>
#include <thread>
#include <unistd.h>

#include "pio.hh"
#include "parser.hh"
#include "basic_multi_dma.hh"
#include "config.h"
#include "elements.hh"
#include "sequence.hh"
#include "readback.hh"
#include "qout.hh"

class mstests {
 public:
   multistreamer &ms;
   qout &q;
   readback &rb;
   pio_out &pio_trig_int;
   InputParser &input;
   Verbosity &verb;

   mstests(multistreamer &_ms, qout &_q, readback &_rb, pio_out &_pio_trig_int, InputParser &_input, Verbosity &_v) :
          ms(_ms), q(_q), rb(_rb), pio_trig_int(_pio_trig_int), input(_input), verb(_v) {}

   static void trig1(pio_out &p, InputParser &input) {
     if (input.exists("-trig")) {
       const int delay = 100*1000;
       usleep(delay);
       p.write(1);
       usleep(delay);
       p.write(0);
       std::cout << "## Trigger sequence emitted." << std::endl;
     }
   }

   int test1(multistreamer &ms, qout &q, readback &rb, InputParser &input, pio_out &pio) {
     std::cout << "test1 - one or another" << std::endl;
     const auto c = parse_count(input, "-c", "10");
     auto prepare = [&](int i, basic_streamer &st) {
       Sequence s;
       s.push_back(el(0b1, 0b1, true)); // pattern, mask
       const auto v = random_value();
       s.push_back(el(c, v));
       s.push_back(el());
       if (verb.veryverbose) s.dump(std::cout, std::to_string(i) + "| ");
       st.fifo.send_sequence(s);
       st.sc.trigger_enable();
       st.sc.status_report();
       return std::make_pair(v, s);
     };
     auto [v1, el1] = prepare(1, ms.s1);
     auto [v2, el2] = prepare(2, ms.s2);
     auto [v3, el3] = prepare(3, ms.s3);
     auto [v4, el4] = prepare(4, ms.s4);
     auto cm = ndx2mode(input.get_uint32("-mode", 1));
     q.cq.mode(cm);
     value_t result = combine(cm, v1, v2, v3, v4);
     Sequence el_ref;
     el_ref.push_back(el(c, result));
     el_ref.push_back(el());
     if (verb.veryverbose)
       std::cout << "comb_mode=" << to_string(cm) << " result=0x" << std::hex << result << std::endl;
     std::thread trig(trig1, std::ref(pio), std::ref(input));
     int rc = 0;
     if (input.exists("-check")) {
       usleep(10);
       if (verb.veryverbose) rb.check_fill_status();
       const double timeout = 1.0;
       int successful = rb.check(el_ref, timeout);
       if (!successful)
         rc |= 1;
     }
     trig.join();
     return rc;
   }

   int run(int test) {
     std::cout << "Requested test " << std::dec << test << std::endl;
     int rc = 0;
     switch (test) {
     case 0:
       std::cout << "There is nothing to do!" << std::endl;
       break;
     case 1:
       rc = test1(ms, q, rb, input, pio_trig_int);
       break;
     default:
       std::cerr << "Unknown test " << test << std::endl;
       break;
     }
     return rc;
   }
};
