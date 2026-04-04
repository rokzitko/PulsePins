# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

import pytest
import pp
import pp_impl
import tempfile
from pathlib import Path

import time
usleep = lambda x: time.sleep(x/1000000.0)

def test_the_answer():
   assert pp.the_answer == 42

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
   e = pp.el(pp.el_type.regular, c, v)
   assert e.control() == 0
   assert e.count() == 1
   assert e.value() == 2

def test_el2():
   c = pp.Counter(1)
   v = pp.Value(2)
   y = 3
   e = pp.el(pp.el_type.regular, c, v, y)
   assert e.control() == 3
   assert e.count() == 1
   assert e.value() == 2

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
   assert e.control() == 0
   assert e.count() == 1
   assert e.value() == 2

def test_el7_counter_and_int_value():
   c = pp.Counter(1)
   v = 2
   e = pp.el(c,v)
   assert e.control() == 0
   assert e.count() == 1
   assert e.value() == 2

def test_el7_counter_and_value():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(c,v)
   assert e.control() == 0
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

def test_set_control():
   c = pp.Counter(1)
   v = pp.Value(2)
   y = 3
   e = pp.el(pp.el_type.regular, c, v, y)
   assert e.control() == 3
   e.set_control(4)
   assert e.control() == 4

def test_set_count():
   c = pp.Counter(1)
   v = pp.Value(2)
   y = 3
   e = pp.el(pp.el_type.regular, c, v, y)
   assert e.count() == 1
   e.set_count(pp.Counter(4))
   assert e.count() == 4

def test_set_value():
   c = pp.Counter(1)
   v = pp.Value(2)
   y = 3
   e = pp.el(pp.el_type.regular, c, v, y)
   assert e.value() == 2
   e.set_value(pp.Value(4))
   assert e.value() == 4

def test_updated_value():
   c = pp.Counter(1)
   v = pp.BitSet(2)
   e = pp.el(pp.el_type.regular, c, v)
   assert e.value() == 2
   assert e.updated_value(1) == 3

def test_is_regular():
   c = pp.Counter(1)
   v = pp.Value(2)
   e = pp.el(pp.el_type.regular, c, v)
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
   e1 = pp.el(pp.el_type.regular, c1, v1)
   e1bis = pp.el(pp.el_type.regular, c1, v1)
   c2 = pp.Counter(2)
   v2 = pp.Value(4)
   e2 = pp.el(pp.el_type.regular, c2, v2)
   assert e1 == e1bis
   assert e1 != e2

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

def test_check_firmware():
   pp.check_firmware()

def test_mm():
   assert pp_impl.LWHPSFPGA_OFST == 0xFF200000
   assert pp_impl.LWH2F_RANGE == pp_impl.LWHPSFPGA_END - pp_impl.LWHPSFPGA_OFST
   dev_lw = pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)
   loc = dev_lw.get_loc(pp_impl.ST_INTERFACE_1_BASE, 0)

# check_ID() from misc.hh
def test_check_ID():
   dev = pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)
   id1 = pp.sysid(dev, pp_impl.SYSID_BASE, pp_impl.SYSID_ID)

def test_streamer_control():
   dev_h2f = pp.mm(pp_impl.HPSFPGA_OFST, pp_impl.H2F_RANGE)
   sc = pp.streamer_control(dev_h2f, pp_impl.ST_INTERFACE_1_BASE)

def test_sc_initial_value():
   dev_h2f = pp.mm(pp_impl.HPSFPGA_OFST, pp_impl.H2F_RANGE)
   sc = pp.streamer_control(dev_h2f, pp_impl.ST_INTERFACE_1_BASE)
   v = 42
   sc.set_initial_value(v)
   sc.reset() # required
   v2 = sc.get_qout()
   assert v == v2

def test_sc_reset():
   dev_h2f = pp.mm(pp_impl.HPSFPGA_OFST, pp_impl.H2F_RANGE)
   sc = pp.streamer_control(dev_h2f, pp_impl.ST_INTERFACE_1_BASE)
   fifo = pp.streamer_fifo(dev_h2f, pp_impl.FIFO_1_IN_BASE, pp_impl.FIFO_1_IN_CSR_BASE)
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

def test_FPGA():
   params = ["-test"]
   input = pp.InputParser(params)
   v = pp.Verbosity()
   v.veryverbose = True
   v.verbosecheck = True
   fpga = pp.FPGA(v)
#   fpga.mgr.status()
   fpga.status()

def test_pll():
    dev_lw  = pp.mm(pp_impl.LWHPSFPGA_OFST, pp_impl.LWH2F_RANGE)
    pll = pp.pll(dev_lw, pp_impl.PLL_RECONFIG_INT_CLK_BASE)
    pll.set_M(32,True)
    pll.set_N(5,True)
    pll.set_C(8,True,0)
