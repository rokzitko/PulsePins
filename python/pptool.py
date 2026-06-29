# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

import pp
import pp_impl

import time
usleep = lambda x: time.sleep(x/1000000.0)

DEFAULT_READBACK_TIMEOUT_S = 2.0
DEFAULT_COMPLETION_TIMEOUT_S = 10.0

class pptool:
    def __init__(self):
        self.ip = pp.InputParser( [] )
        self.dev_lw  = pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)
        self.dev_h2f = pp.mm(pp_impl.HPSFPGA_OFST,   pp_impl.H2F_RANGE)
        self.sc = pp.streamer_control(self.dev_h2f, pp_impl.ST_INTERFACE_1_BASE)
        self.fifo = pp.streamer_fifo(self.dev_h2f, pp_impl.FIFO_1_IN_BASE, pp_impl.FIFO_1_IN_CSR_BASE)
        self.initial_value = 0
        self.verbose = True
        self.v = pp.Verbosity()
        self.v.veryverbose = True
        self.v.verbosecheck = True
        self.fpga = pp.FPGA(self.v)
        self.fm = pp.pp_freq_meter(self.ip, self.fpga, True)
        self.fm.report();
        self.rb = pp.readback(self.fpga, self.dev_h2f, pp_impl.FIFO_RL_OUT_BASE, pp_impl.FIFO_RL_IN_CSR_BASE, pp_impl.RL_ENCODER_IF_BASE)
        self.pio0 = pp.pio_out(self.dev_lw, pp_impl.PIO_TRIG_INT_BASE)
        self.pio0.write(0)

    def init(self):
        self.sc.set_initial_value(self.initial_value)
        self.sc.reset()
        self.rb.reset()
        self.rb.check_fill_status()

    def send_and_check(self, elements, timeout = DEFAULT_READBACK_TIMEOUT_S):
        self.fifo.send_sequence(elements)
        usleep(100)
        self.sc.trigger_force()
        success = self.rb.check(elements, timeout)
        if not success:
            raise RuntimeError("Readback check failed.")
        start = time.monotonic()
        while not self.sc.done():
            if self.sc.buffer_error():
                raise RuntimeError("Streamer buffer error detected.")
            if DEFAULT_COMPLETION_TIMEOUT_S > 0 and (time.monotonic() - start) > DEFAULT_COMPLETION_TIMEOUT_S:
                raise TimeoutError("Timeout waiting for streamer completion.")
            usleep(1)
        if self.sc.buffer_error():
            raise RuntimeError("Streamer buffer error detected.")
        final_qout = self.sc.get_qout()
        if final_qout != 0:
            raise RuntimeError(f"Unexpected final qout: {final_qout}")
