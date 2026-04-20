# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

import pytest
import pp
import pp_impl
import tempfile
from pathlib import Path

import time
usleep = lambda x: time.sleep(x/1000000.0)


@pytest.fixture(scope="session")
def verbosity():
   return pp.Verbosity()


@pytest.fixture(scope="session")
def fpga(verbosity):
   board = pp.FPGA(verbosity)
   opts = pp.InputParser([])
   # Mirror the normal host-runtime startup path and ensure streamer clock is measured before
   # wrappers such as readback use streamer-clock-based waits.
   try:
      pp.pp_freq_meter(opts, board, True)
   except RuntimeError as exc:
      if "freq_meter" not in str(exc):
         raise
      # Some deployed images expose a different frequency-meter layout. The readback smoke
      # tests only need a conservative streamer-clock estimate for reset-settling sleeps.
      board.set_streamer_clk(100e6)
   return board


@pytest.fixture(scope="session")
def dev_h2f():
   return pp.mm(pp_impl.HPSFPGA_OFST, pp_impl.H2F_RANGE)


@pytest.fixture(scope="session")
def dev_lw():
   return pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)


def make_readback(fpga, dev_h2f):
   rb = pp.readback(fpga, dev_h2f,
                    pp_impl.FIFO_RL_OUT_BASE,
                    pp_impl.FIFO_RL_IN_CSR_BASE,
                    pp_impl.RL_ENCODER_IF_BASE)
   return rb


def make_streamer_fifo(dev_h2f):
   fifo = pp.streamer_fifo(dev_h2f, pp_impl.FIFO_1_IN_BASE, pp_impl.FIFO_1_IN_CSR_BASE)
   return fifo


def make_streamer_control(dev_h2f):
   sc = pp.streamer_control(dev_h2f, pp_impl.ST_INTERFACE_1_BASE)
   return sc

def test_the_answer():
   assert pp.the_answer == 42


def test_InputParser_exists_and_get():
   p = pp.InputParser(["-a", "1", "-b", "hello"])
   assert p.exists("-a") is True
   assert p.exists("-b") is True
   assert p.exists("-c") is False
   assert p.get("-a") == "1"
   assert p.get("-b") == "hello"


def test_InputParser_get_string_default():
   p = pp.InputParser([])
   assert p.get_string("-missing", "fallback") == "fallback"


def test_InputParser_get_double_and_uint32():
   p = pp.InputParser(["-x", "1.25", "-y", "42"])
   assert p.get_double("-x", 0.0) == 1.25
   assert p.get_uint32("-y", 0) == 42


def test_InputParser_add_and_add_with_arg():
   p = pp.InputParser([])
   p.add("-flag")
   p.add_with_arg("-name", "value")
   assert p.exists("-flag") is True
   assert p.get_string("-name", "") == "value"

def test_Counter():
   c = pp.Counter(10)
   assert c.count() == 10
   assert c.control_bits() == 0
   assert c.desc() == ""

def test_Strobe():
   c = pp.Strobe(11)
   assert c.count() == 11
   assert c.control_bits() == pp_impl.STROBE
   assert c.desc() == pp_impl.strobestring

def test_NoStrobe():
   c = pp.NoStrobe(12)
   assert c.count() == 12
   assert c.control_bits() == pp_impl.NOSTROBE
   assert c.desc() == pp_impl.nostrobestring

def test_Value():
   v = pp.Value(10)
   assert v.value() == 10
   assert v.result(0) == 10
   assert v.mode_bits() == 0
   assert v.desc() == ""

def test_BitLoad():
   v = pp.BitLoad(10)
   assert v.value() == 10
   assert v.result(0) == 10
   assert v.result(1) == 10
   assert v.result(10) == 10
   assert v.mode_bits() == pp_impl.BITLOAD
   assert v.desc() == pp_impl.bitloadstring

def test_BitSet():
   v = pp.BitSet(10)
   assert v.value() == 10
   assert v.result(0) == 10
   assert v.result(1) == 11
   assert v.result(10) == 10
   assert v.mode_bits() == pp_impl.BITSET
   assert v.desc() == pp_impl.bitsetstring

def test_BitClear():
   v = pp.BitClear(10)
   assert v.value() == 10
   assert v.result(0) == 0
   assert v.result(1) == 1
   assert v.result(10) == 0
   assert v.mode_bits() == pp_impl.BITCLEAR
   assert v.desc() == pp_impl.bitclearstring

def test_BitFlip():
   v = pp.BitFlip(10)
   assert v.value() == 10
   assert v.result(0) == 10
   assert v.result(1) == 11
   assert v.result(10) == 0
   assert v.mode_bits() == pp_impl.BITFLIP
   assert v.desc() == pp_impl.bitflipstring

def test_BitNot():
   v = pp.BitNot(0)
   assert v.value() == 0
   assert v.result(0) == 0xFFFFFFFF
   assert v.result(0xFFFFFFFF) == 0
   assert v.mode_bits() == pp_impl.BITNOT
   assert v.desc() == pp_impl.bitnotstring

def test_BitAnd():
   v = pp.BitAnd(2)
   assert v.value() == 2
   assert v.result(0) == 0
   assert v.result(1) == 0
   assert v.result(2) == 2
   assert v.mode_bits() == pp_impl.BITAND
   assert v.desc() == pp_impl.bitandstring

def test_BitOr():
   v = pp.BitOr(2)
   assert v.value() == 2
   assert v.result(0) == 2
   assert v.result(1) == 3
   assert v.result(2) == 2
   assert v.mode_bits() == pp_impl.BITOR
   assert v.desc() == pp_impl.bitorstring

def test_BitXor():
   v = pp.BitXor(2)
   assert v.value() == 2
   assert v.result(0) == 2
   assert v.result(1) == 3
   assert v.result(2) == 0
   assert v.mode_bits() == pp_impl.BITXOR
   assert v.desc() == pp_impl.bitxorstring

def test_BitXnor():
   v = pp.BitXnor(2)
   assert v.value() == 2
   assert v.result(0) == 0xFFFFFFFF - 2
   assert v.result(1) == 0xFFFFFFFF - 3
   assert v.result(2) == 0xFFFFFFFF
   assert v.mode_bits() == pp_impl.BITXNOR
   assert v.desc() == pp_impl.bitxnorstring

def test_BitSll():
   v = pp.BitSll(2)
   assert v.value() == 2
   assert v.result(0) == 0
   assert v.result(1) == 4
   assert v.result(2) == 8
   assert v.mode_bits() == pp_impl.BITSLL
   assert v.desc() == pp_impl.bitsllstring

def test_BitSrl():
   v = pp.BitSrl(2)
   assert v.value() == 2
   assert v.result(0) == 0
   assert v.result(1) == 0
   assert v.result(2) == 0
   assert v.result(4) == 1
   assert v.mode_bits() == pp_impl.BITSRL
   assert v.desc() == pp_impl.bitsrlstring

def test_TriggerCondition():
   v = pp.TriggerCondition(1,1,False)
   assert v.value() == (1 << pp_impl.WIDTH_TRIGGER) + 1
   assert v.mode_bits() == pp_impl.TRIGGER
   assert v.desc() == pp_impl.triggerstring

def test_TriggerConditionFinal():
   v = pp.TriggerCondition(2,2,True)
   assert v.value() == (2 << pp_impl.WIDTH_TRIGGER) + 2
   assert v.mode_bits() == pp_impl.TRIGGER + pp_impl.TRIGGERFINAL
   assert v.desc() == pp_impl.triggerstring + pp_impl.finalstring

def test_el1():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(c, v)
   assert e.control() == pp_impl.BITLOAD
   assert e.count() == 1
   assert e.value() == 2
   assert e.kind() == pp.el_type.regular

def test_el2():
   e = pp.el.from_raw_triplet(pp_impl.BITSET | pp_impl.NOSTROBE, 1, 2)
   assert e.control() == pp_impl.BITSET | pp_impl.NOSTROBE
   assert e.count() == 1
   assert e.value() == 2
   assert e.no_strobe()
   assert e.kind() == pp.el_type.regular

def test_el3():
   e = pp.el()
   assert e.control() == pp_impl.TERMINATE
   assert e.count() == 1
   assert e.value() == 0

def test_el4():
   e = pp.el(42)
   assert e.control() == pp_impl.TERMINATE
   assert e.count() == 1
   assert e.value() == 42

def test_el7_count_and_int_value():
   c = 1
   v = 2
   e = pp.el(c,v)
   assert e.control() == pp_impl.BITLOAD
   assert e.count() == 1
   assert e.value() == 2

def test_el7_counter_and_int_value():
   c = pp.Counter(1)
   v = 2
   e = pp.el(c,v)
   assert e.control() == pp_impl.BITLOAD
   assert e.count() == 1
   assert e.value() == 2

def test_el7_counter_and_value():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(c,v)
   assert e.control() == pp_impl.BITLOAD
   assert e.count() == 1
   assert e.value() == 2

def test_el8():
   p = 1
   m = 2
   b = False
   e = pp.el(p,m,b)
   assert e.control() == pp_impl.TRIGGER
   assert e.count() == 0
   assert e.value() == (m << pp_impl.WIDTH_TRIGGER) + p

def test_el9():
   p = 1
   m = 2
   b = True
   e = pp.el(p,m,b)
   assert e.control() == pp_impl.TRIGGER | pp_impl.TRIGGERFINAL
   assert e.count() == 0
   assert e.value() == (m << pp_impl.WIDTH_TRIGGER) + p

def test_el10():
   r = 1
   l = 2
   e = pp.el(pp.Replay(),r,l)
   assert e.control() == pp_impl.REPLAY
   assert e.count() == r
   assert e.value() == l

def test_el11_retrig():
   e = pp.el(pp.Retrig())
   assert e.control() == pp_impl.RETRIG
   assert e.count() == 1
   assert e.value() == pp_impl.default_final_value
   assert e.kind() == pp.el_type.retrig

def test_el12_prng():
   e = pp.el(pp.PseudoRandom(), 9)
   assert e.control() == pp_impl.PRNG
   assert e.count() == 9
   assert e.value() == 0
   assert e.kind() == pp.el_type.prng

def test_store1():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(c,v)
   position = 3
   e.store(position)
   assert e.control() == pp_impl.STORE + (position << pp_impl.SHIFT_POSITION)
   assert e.count() == 1
   assert e.value() == 2

def test_store2():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(c,v)
   position = pp_impl.POSITIONS
   with pytest.raises(Exception):
       e.store(position)

def test_stored_in():
   e = pp.el(1, 2)
   e2 = e.stored_in(3)
   assert e.control() == pp_impl.BITLOAD
   assert e2.control() == pp_impl.STORE + (3 << pp_impl.SHIFT_POSITION)
   assert e2.store_slot() == 3

def test_set_control():
   e = pp.el(1, 2)
   e.set_control(pp_impl.BITSET)
   assert e.control() == pp_impl.BITSET
   assert e.kind() == pp.el_type.regular

def test_set_count():
   e = pp.el(1, 2)
   assert e.count() == 1
   e.set_count(pp.Counter(4))
   assert e.count() == 4

def test_set_value():
   e = pp.el(1, 2)
   assert e.value() == 2
   e.set_value(pp.BitXor(4))
   assert e.value() == 4
   assert e.mode() == pp_impl.BITXOR

def test_updated_value():
   c = pp.Counter(1)
   v = pp.BitSet(2)
   e = pp.el(c, v)
   assert e.value() == 2
   assert e.updated_value(1) == 3

def test_is_regular():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(c, v)
   assert e.is_regular()

def test_is_trigger():
   p = 1
   m = 2
   b = True
   e = pp.el(p,m,b)
   assert e.is_trigger()

def test_is_final():
   e = pp.el()
   assert e.is_final()

def test_decode():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(c,v)
   position = 3
   e.store(position)
   assert e.decode() == " (store 3)"

def test_eq():
   c1 = pp.Counter(1)
   v1 = pp.Value(2)
   e1 = pp.el(c1, v1)
   e1bis = pp.el(c1, v1)
   c2 = pp.Counter(2)
   v2 = pp.Value(4)
   e2 = pp.el(c2, v2)
   assert e1 == e1bis
   assert e1 != e2

def test_element_helpers():
   e = pp.el(pp.Counter(3), pp.BitSet(0x12))
   assert e.no_strobe() == False
   assert e.regular_token() == "s"
   assert e.sequence_record() == "s 3 0x12"

   changed = e.with_count(7)
   assert changed.count() == 7
   assert e.count() == 3

   changed2 = e.with_counter(pp.Counter(5))
   assert changed2.count() == 5
   assert changed2.no_strobe() == False

   changed3 = e.with_regular_value(pp.BitXor(0x03))
   assert changed3.mode() == pp_impl.BITXOR
   assert changed3.updated_value(0x04) == 0x07

   changed4 = e.as_bitload_after(0x01)
   assert changed4.mode() == pp_impl.BITLOAD
   assert changed4.value() == 0x13

   no_strobe = pp.el(pp.NoStrobe(3), 0x12)
   assert no_strobe.no_strobe()
   assert no_strobe.regular_token() == "dn"
   assert no_strobe.sequence_record() == "dn 3 0x12"

def test_element_static_helpers():
   assert pp.el.classify_control(pp_impl.BITLOAD) == pp.el_type.regular
   assert pp.el.classify_control(pp_impl.REPLAY) == pp.el_type.replay
   assert pp.el.is_regular_token("xr")
   assert pp.el.is_regular_token("dn")
   assert pp.el.is_regular_token("t") == False

   e = pp.el.from_regular_token("xr", 7, 0x12)
   assert e.mode() == pp_impl.BITXOR
   assert e.regular_token() == "xr"

   e2 = pp.el.from_raw_triplet(pp_impl.TRIGGER | pp_impl.TRIGGERFINAL, 0, (0x2a << pp_impl.WIDTH_TRIGGER) + 0x55)
   assert e2.is_trigger()
   assert e2.trigger_pattern() == 0x55
   assert e2.trigger_mask() == 0x2a
   assert e2.trigger_is_final()

def test_parse_sequence_text_roundtrip_regular():
   seq = pp.Sequence()
   seq.push_back(pp.el(3, 0x12))
   seq.push_back(pp.el(4, 0x34))

   text = pp.write_sequence_text(seq)
   seq2, force_trigger = pp.parse_sequence_text(text)

   assert force_trigger == False
   assert pp.write_sequence_text(seq2) == text

def test_parse_sequence_text_roundtrip_control_flow():
   text = "tn 0x1 0x3\nstore 2 d 5 0x12\nr 7 0x4\nrt\npr 9\nt 0x2 0x3\nfinal 0x34\n"

   seq, force_trigger = pp.parse_sequence_text(text)

   assert force_trigger == False
   assert pp.write_sequence_text(seq) == text

def test_sequence_vcd_roundtrip_bitload():
   seq = pp.Sequence()
   seq.push_back(pp.el(3, 0x1))
   seq.push_back(pp.el(2, 0x3))
   seq.push_back(pp.el(4, 0x0))

   with tempfile.TemporaryDirectory() as tmpdir:
      path = Path(tmpdir) / "roundtrip.vcd"
      seq.write_VCD_file(str(path), "outs", "1ns")
      seq2 = pp.Sequence()
      seq2.load_VCD(str(path), "outs", 1)
      assert pp.write_sequence_text(seq2) == pp.write_sequence_text(seq)

def test_sequence_vcd_export_rejects_trigger():
   seq = pp.Sequence()
   seq.push_back(pp.el(0x1, 0x3, True))

   with tempfile.TemporaryDirectory() as tmpdir:
      path = Path(tmpdir) / "bad.vcd"
      with pytest.raises(RuntimeError):
         seq.write_VCD_file(str(path), "outs", "1ns")

def test_sequence_vcd_export_rejects_replay():
   seq = pp.Sequence()
   seq.push_back(pp.el(pp.Replay(), 3, 4))

   with tempfile.TemporaryDirectory() as tmpdir:
      path = Path(tmpdir) / "bad.vcd"
      with pytest.raises(RuntimeError):
         seq.write_VCD_file(str(path), "outs", "1ns")

def test_sequence_vcd_export_empty_sequence():
   seq = pp.Sequence()

   with tempfile.TemporaryDirectory() as tmpdir:
      path = Path(tmpdir) / "empty.vcd"
      seq.write_VCD_file(str(path), "outs", "1ns")
      data = path.read_text()
      assert "$scope module pulsepins $end" in data
      assert "#0" in data

def test_sequence_binary_roundtrip_regular():
   seq = pp.Sequence()
   seq.push_back(pp.el(3, 0x12))
   seq.push_back(pp.el(pp.NoStrobe(4), 0x34))

   with tempfile.TemporaryDirectory() as tmpdir:
      path = Path(tmpdir) / "roundtrip.ppbin"
      seq.write_binary_file(str(path), False)
      seq2, force_trigger = pp.read_sequence_binary(str(path))
      assert force_trigger == False
      assert pp.write_sequence_text(seq2) == pp.write_sequence_text(seq)

def test_sequence_binary_roundtrip_control_flow_and_force_trigger():
   text = "tn 0x1 0x3\nstore 2 d 5 0x12\nr 7 0x4\nrt\npr 9\nt 0x2 0x3\nfinal 0x34\n"
   seq, _ = pp.parse_sequence_text(text)

   with tempfile.TemporaryDirectory() as tmpdir:
      path = Path(tmpdir) / "roundtrip.ppbin"
      seq.write_binary_file(str(path), True)
      seq2, force_trigger = pp.read_sequence_binary(str(path))
      assert force_trigger == True
      assert pp.write_sequence_text(seq2) == pp.write_sequence_text(seq)


def test_readback_mode_set_smoke(fpga, dev_h2f):
   rb = make_readback(fpga, dev_h2f)
   rb.mode(0)
   rb.mode(1)


def test_readback_status_report_smoke(fpga, dev_h2f):
   rb = make_readback(fpga, dev_h2f)
   rb.status_report()


def test_readback_check_fill_status_smoke(fpga, dev_h2f):
   rb = make_readback(fpga, dev_h2f)
   rb.check_fill_status()


def test_readback_clear_fifo_and_reset_smoke(fpga, dev_h2f):
   rb = make_readback(fpga, dev_h2f)
   rb.clear_fifo()
   rb.reset()


def test_readback_filled_and_overflow_smoke(fpga, dev_h2f):
   rb = make_readback(fpga, dev_h2f)
   assert isinstance(bool(rb.filled()), bool)
   assert isinstance(bool(rb.overflow()), bool)


def test_streamer_fifo_report_smoke(dev_h2f):
   fifo = make_streamer_fifo(dev_h2f)
   fifo.report()


def test_streamer_fifo_check_fill_status_smoke(dev_h2f):
   fifo = make_streamer_fifo(dev_h2f)
   fifo.check_fill_status()


def test_sc_qout_set_and_select_smoke(dev_h2f):
   sc = make_streamer_control(dev_h2f)
   sc.qout_set(0x1234)
   sc.qout_select(True)
   sc.qout_select(False)


def test_sc_trigger_controls_smoke(dev_h2f):
   sc = make_streamer_control(dev_h2f)
   sc.trigger_enable()
   sc.trigger_force()
   sc.trigger_reset()


def test_sc_stop_on_buffer_error_smoke(dev_h2f):
   sc = make_streamer_control(dev_h2f)
   sc.stop_on_buffer_error(True)
   sc.stop_on_buffer_error(False)


def test_sc_gating_smoke(dev_h2f):
   sc = make_streamer_control(dev_h2f)
   sc.gating(True, False, 0)
   sc.gating(True, True, 0x1)


def test_sc_gate_status_helpers_smoke(dev_h2f):
   sc = make_streamer_control(dev_h2f)
   sc.gate_status()
   sc.gate_status_string()
   sc.gate_status_string_from_x(0)

def test_check_firmware():
   pp.check_firmware()

def test_mm(dev_lw):
   assert pp_impl.LWHPSFPGA_OFST == 0xFF200000
   assert pp_impl.LWH2F_RANGE == pp_impl.LWHPSFPGA_END - pp_impl.LWHPSFPGA_OFST
   loc = dev_lw.get_loc(pp_impl.ST_INTERFACE_1_BASE, 0)

# check_ID() from misc.hh
def test_check_ID():
   dev = pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)
   pp.sysid(dev, pp_impl.SYSID_BASE, pp_impl.SYSID_ID)

def test_streamer_control(dev_h2f):
   pp.streamer_control(dev_h2f, pp_impl.ST_INTERFACE_1_BASE)

def test_sc_initial_value(dev_h2f):
   sc = make_streamer_control(dev_h2f)
   v = 42
   sc.set_initial_value(v)
   sc.reset() # required
   v2 = sc.get_qout()
   assert v == v2

def test_sc_reset(dev_h2f):
   sc = make_streamer_control(dev_h2f)
   fifo = make_streamer_fifo(dev_h2f)
   sc.reset()
   st = sc.status()
   assert (st & 0b1111) == 0 # Use the mask!
   co = sc.get_control()
   assert co == 0
   assert sc.buffer_error() == False
   assert sc.done() == False

def test_Verbosity():
   v = pp.Verbosity()
   v.veryverbose = True
   v.verbosecheck = True

def test_FPGA(fpga):
   fpga.status()

def test_pll(dev_lw):
   pll = pp.pll(dev_lw, pp_impl.PLL_RECONFIG_INT_CLK_BASE)
   pll.set_M(32,True)
   pll.set_N(5,True)
   pll.set_C(8,True,0)
