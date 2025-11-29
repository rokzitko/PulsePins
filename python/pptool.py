# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

import pp
import pp_impl

import time
usleep = lambda x: time.sleep(x/1000000.0)

class pptool:
    def __init__(self):
        self.dev_lw  = pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)
        self.dev_h2f = pp.mm(pp_impl.HPSFPGA_OFST,   pp_impl.H2F_RANGE)
        self.sc = pp.streamer_control(self.dev_h2f, pp_impl.ST_INTERFACE_1_BASE)
        self.fifo = pp.streamer_fifo(self.dev_h2f, pp_impl.FIFO_1_IN_BASE, pp_impl.FIFO_1_IN_CSR_BASE)
        self.initial_value = 0
        self.verbose = True
        self.v = pp.Verbosity()
        self.v.veryverbose = True
        self.v.verbosecheck = True
        self.rb = pp.readback(self.dev_h2f, pp_impl.FIFO_RL_OUT_BASE, pp_impl.FIFO_RL_IN_CSR_BASE, pp_impl.RL_ENCODER_IF_BASE, self.v)
        self.pio0 = pp.pio_out(self.dev_lw, pp_impl.PIO_TRIG_INT_BASE)
        self.pio0.write(0)

    def init(self):
        self.sc.set_initial_value(self.initial_value)
        self.sc.reset(self.verbose)
        self.rb.reset()
        self.rb.check_fill_status()

    def send_and_check(self, elements, timeout = 0):
        self.fifo.send_sequence(elements)
        usleep(100)
        self.sc.trigger_force()
        success = self.rb.check(elements, timeout)
        assert success
        while not(self.sc.done() or self.sc.buffer_error()):
            usleep(1)
        final_qout = self.sc.get_qout()
        assert final_qout == 0
