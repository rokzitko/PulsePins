#!/usr/bin/env python3

import pp
import pp_impl

import time
usleep = lambda x: time.sleep(x/1000000.0)

DEFAULT_READBACK_TIMEOUT_S = 2.0
DEFAULT_COMPLETION_TIMEOUT_S = 10.0

ip = pp.InputParser( [] )
dev_lw  = pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)
dev_h2f = pp.mm(pp_impl.HPSFPGA_OFST,   pp_impl.H2F_RANGE)
sc = pp.streamer_control(dev_h2f, pp_impl.ST_INTERFACE_1_BASE)
fifo = pp.streamer_fifo(dev_h2f, pp_impl.FIFO_1_IN_BASE, pp_impl.FIFO_1_IN_CSR_BASE)
initial_value = 0
sc.set_initial_value(initial_value)
verbose = True
sc.reset()
v = pp.Verbosity()
v.veryverbose = True
v.verbosecheck = True
fpga = pp.FPGA(v)
fm = pp.pp_freq_meter(ip, fpga, True)
fm.report();
rb = pp.readback(fpga, dev_h2f, pp_impl.FIFO_RL_OUT_BASE, pp_impl.FIFO_RL_IN_CSR_BASE, pp_impl.RL_ENCODER_IF_BASE)
rb.reset()
rb.check_fill_status()
pio0 = pp.pio_out(dev_lw, pp_impl.PIO_TRIG_INT_BASE)
pio0.write(0)
elements = pp.Sequence()
c = 1
v = 0b11
elements.push_back(pp.el(c, v))
elements.push_back(pp.el())
fifo.send_sequence(elements)
usleep(100)
sc.trigger_force()
timeout = DEFAULT_READBACK_TIMEOUT_S
success = rb.check(elements, timeout)
assert success
start = time.monotonic()
while not(sc.done() or sc.buffer_error()):
    if DEFAULT_COMPLETION_TIMEOUT_S > 0 and (time.monotonic() - start) > DEFAULT_COMPLETION_TIMEOUT_S:
        raise TimeoutError("Timeout waiting for streamer completion.")
    usleep(1)
final_qout = sc.get_qout()
assert final_qout == 0
