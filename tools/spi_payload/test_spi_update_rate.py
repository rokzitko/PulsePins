"""Decode the generated bus independently and check acquisition evidence handling."""

import contextlib
import io
import json
import math
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zlib

import spi_update_rate as rate
from check_readback import expand_sequence, merge_runs


def decode(runs):
    previous = 2
    now = 0
    bits = None
    transactions = []
    updates = []
    pulses = []
    update_start = None
    for ticks, pins in runs:
        if previous & 2 and not pins & 2:
            if bits is not None:
                raise AssertionError("Nested CS")
            bits = []
        if not previous & 1 and pins & 1:
            if pins & 2 or bits is None:
                raise AssertionError("Clock outside transaction")
            bits.append((pins >> 2) & 1)
        if not previous & 2 and pins & 2:
            if not bits or len(bits) % 8:
                raise AssertionError("Incomplete SPI frame")
            transactions.append(bytes(sum(bits[i + j] << (7 - j) for j in range(8))
                                      for i in range(0, len(bits), 8)))
            bits = None
        if not previous & 64 and pins & 64:
            if not pins & 2 or pins & 1:
                raise AssertionError("UPDATE while SPI selected/clock high")
            updates.append(now)
            update_start = now
        if previous & 64 and not pins & 64:
            pulses.append(now - update_start)
        if pins & ~0xC7:
            raise AssertionError("Unexpected output pin")
        previous = pins
        now += ticks
    if bits is not None:
        raise AssertionError("Unclosed transaction")
    return transactions, updates, pulses


def decode_forwarded(runs, data_delay_ns, serial_bits=1):
    """Sample an independent, always-running 100 MHz clock before new QOUT arrives.

    Model a positive launch-to-arrival delay relative to the clock at the DDS.
    This checks framing/setup for that assumption, not the physical wiring skew.
    """
    previous = 2
    elapsed = 0
    bits = None
    frames, updates, pulses = [], [], []
    data_changed = -math.inf
    selected = -math.inf
    pending_bits = serial_bits
    for ticks, pins in runs:
        if pins & ~0xFE:
            raise AssertionError("Unexpected output pin or software-driven SCLK")
        if pins & 128:
            serial_bits = pending_bits = 1
        if serial_bits == 1 and pins & 0x38:
            raise AssertionError("Extra SDIO lanes driven before four-bit mode")
        start = elapsed * 10 + data_delay_ns
        stop = (elapsed + ticks) * 10 + data_delay_ns
        if (previous ^ pins) & 0x3C:
            data_changed = start
        if previous & 2 and not pins & 2:
            bits = []
            selected = start
        if not pins & 2:
            for edge in range(math.ceil(start / 10), math.ceil(stop / 10)):
                if edge * 10 - data_changed < 2.2 or edge * 10 - selected < 1.0:
                    raise AssertionError("DDS data/CS setup violation in timing model")
                bits.extend((pins >> (2 + lane)) & 1 for lane in reversed(range(serial_bits)))
        if not previous & 2 and pins & 2:
            if not bits or len(bits) % 8:
                raise AssertionError("Incomplete SPI frame")
            frames.append(bytes(sum(bits[i + j] << (7 - j) for j in range(8))
                                for i in range(0, len(bits), 8)))
            if frames[-1][0] == 0:
                if len(frames[-1]) != 2 or frames[-1][1] & 7 not in (0, 6):
                    raise AssertionError("Invalid CSR mode selection")
                pending_bits = 4 if frames[-1][1] & 6 else 1
            bits = None
        if not previous & 64 and pins & 64:
            if not pins & 2:
                raise AssertionError("UPDATE while SPI selected")
            updates.append(elapsed)
            serial_bits = pending_bits
        if previous & 64 and not pins & 64:
            pulses.append(elapsed - updates[-1])
        previous = pins
        elapsed += ticks
    if bits is not None:
        raise AssertionError("Unclosed transaction")
    return frames, updates, pulses


class RateTest(unittest.TestCase):
    def test_every_word_and_update_at_all_requested_clocks(self):
        values = rate.frequencies(100e6, 101e6, 100)
        for clock, half in ((10e6, 5), (12.5e6, 4), (50e6 / 3, 3), (25e6, 2), (50e6, 1)):
            with self.subTest(clock=clock):
                waveform = rate.Ad9959Waveform(clock)
                waveform.sweep(values)
                frames, updates, pulses = decode(waveform.runs)
                self.assertEqual(len(frames), 100)
                self.assertEqual(len(updates), 100)
                self.assertEqual(pulses, [2] * 100)
                for frequency, frame in zip(values, frames):
                    self.assertEqual(len(frame), 5)
                    self.assertEqual(frame[0], 4)
                    realized = int.from_bytes(frame[1:], "big") * 500e6 / 2**32
                    self.assertLessEqual(abs(realized - frequency), 500e6 / 2**33)
                self.assertEqual(set(b - a for a, b in zip(updates, updates[1:])), {80 * half + 12})
                for ticks, pins in waveform.runs:
                    if pins & 1:
                        self.assertEqual(ticks, half)

    def test_full_amplitude_setup_selects_only_ch1_for_bursts(self):
        waveform = rate.Ad9959Waveform(10e6)
        waveform.initialize()
        frames, updates, pulses = decode(waveform.runs)
        self.assertEqual(pulses, [100, 2])
        self.assertGreater(updates[1] - updates[0], 200000)
        self.assertIn(bytes.fromhex("01 d0 00 00"), frames)
        self.assertIn(bytes.fromhex("03 00 03 01"), frames)
        self.assertIn(bytes.fromhex("06 00 13 ff"), frames)
        self.assertEqual(frames[-1], bytes.fromhex("00 20"))
        self.assertEqual(sum(ticks for ticks, pins in waveform.runs if pins & 128), 100)

    def test_saved_crc_covers_every_sample_and_excludes_final_control(self):
        waveform = rate.Ad9959Waveform(10e6)
        waveform.sweep([100e6, 101e6])
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "test.seq"
            info = waveform.save(path)
            data = b"".join(pins.to_bytes(4, "little") * ticks for ticks, pins in waveform.runs)
            self.assertEqual(info["crc32"], zlib.crc32(data))
            self.assertEqual(info["ticks"], len(data) // 4)
            self.assertEqual(path.read_text().splitlines()[-2:], ["final 0x2", "f"])

    def test_compact_replay_preserves_all_spi_edges_and_timing(self):
        for clock in (10e6, 12.5e6, 50e6 / 3, 25e6, 50e6):
            with self.subTest(clock=clock), tempfile.TemporaryDirectory() as temporary:
                waveform = rate.Ad9959Waveform(clock)
                waveform.sweep(rate.frequencies(100e6, 101e6, 100))
                path = Path(temporary) / "burst.seq"
                info = waveform.save(path)
                expanded, final = expand_sequence(path.read_text())
                self.assertEqual(final, 2)
                self.assertEqual(merge_runs(expanded), waveform.runs)
                self.assertEqual(decode(expanded), decode(waveform.runs))
                self.assertLess(info["encoded_records"], info["records"] * 0.5)
                self.assertEqual(rate.waveform_crc32(expanded), info["crc32"])

    def test_compact_setup_and_nonmatching_runs_preserve_waveform(self):
        with tempfile.TemporaryDirectory() as temporary:
            waveform = rate.Ad9959Waveform(10e6)
            waveform.initialize()
            path = Path(temporary) / "setup.seq"
            waveform.save(path)
            expanded, _ = expand_sequence(path.read_text())
            self.assertEqual(merge_runs(expanded), waveform.runs)
            self.assertEqual(decode(expanded), decode(waveform.runs))
        self.assertEqual(rate.compact_runs([], [(1, 0)]), [])
        self.assertEqual(rate.compact_runs([(100, 2)], [(1, 0)]), ["d 100 0x2"])

    def test_forwarded_clock_frames_and_update_spacing(self):
        values = rate.frequencies(100e6, 101e6, 100)
        waveform = rate.Ad9959Waveform(100e6, "streamer")
        waveform.sweep(values)
        for delay in (0.5, 3.0, 7.5):
            with self.subTest(data_delay_ns=delay):
                frames, updates, pulses = decode_forwarded(waveform.runs, delay)
                self.assertEqual(len(frames), 100)
                self.assertEqual(pulses, [2] * 100)
                self.assertEqual(set(b - a for a, b in zip(updates, updates[1:])), {52})
                for frequency, frame in zip(values, frames):
                    self.assertEqual(len(frame), 5)
                    self.assertEqual(frame[0], 4)
                    realized = int.from_bytes(frame[1:], "big") * 500e6 / 2**32
                    self.assertLessEqual(abs(realized - frequency), 500e6 / 2**33)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "forwarded.seq"
            info = waveform.save(path)
            expanded, final = expand_sequence(path.read_text())
            self.assertEqual(final, 2)
            self.assertEqual(info["ticks"], 5202)
            self.assertEqual(merge_runs(expanded), waveform.runs)
            self.assertEqual(info["crc32"], zlib.crc32(b"".join(
                pins.to_bytes(4, "little") * ticks for ticks, pins in expanded)))

    def test_forwarded_setup_has_same_register_writes_and_reset_timing(self):
        normal = rate.Ad9959Waveform(50e6)
        normal.initialize()
        forwarded = rate.Ad9959Waveform(100e6, "streamer")
        forwarded.initialize()
        frames, updates, pulses = decode_forwarded(forwarded.runs, 3.0)
        self.assertEqual(frames, decode(normal.runs)[0])
        self.assertEqual(pulses, [100, 2])
        self.assertGreater(updates[1] - updates[0], 200000)
        self.assertEqual(sum(ticks for ticks, pins in forwarded.runs if pins & 128), 100)
        # A late data arrival leaves too little setup even if bit counts match.
        with self.assertRaisesRegex(AssertionError, "setup violation"):
            decode_forwarded(forwarded.runs, 8.0)

    def test_actual_clock_and_invalid_parameters(self):
        self.assertAlmostEqual(rate.Ad9959Waveform(20e6).clock, 50e6 / 3)
        for clock in (0, -1, math.nan, math.inf, 51e6):
            with self.assertRaises(ValueError):
                rate.Ad9959Waveform(clock)
        for clock in (0, -1, math.nan, math.inf, 50e6, 101e6):
            with self.assertRaises(ValueError):
                rate.Ad9959Waveform(clock, "streamer")
        with self.assertRaises(ValueError):
            rate.Ad9959Waveform(100e6, "unknown")
        for start, stop, points in ((0, 101e6, 100), (100e6, 100e6, 100),
                                    (100e6, 201e6, 100), (100e6, 101e6, 1)):
            with self.assertRaises(ValueError):
                rate.frequencies(start, stop, points)

    def test_quad_mode_switch_and_nibble_mapping(self):
        waveform = rate.Ad9959Waveform(100e6, "streamer", 4)
        waveform.initialize()
        waveform.sweep([100e6, 101e6])
        frames, updates, pulses = decode_forwarded(waveform.runs, 3.0)
        self.assertEqual(frames[-3], bytes.fromhex("00 26"))
        self.assertEqual(frames[-2:], [bytes.fromhex("04 33 33 33 33"), bytes.fromhex("04 33 b6 45 a2")])
        self.assertEqual(pulses, [100, 2, 2, 2, 2])
        self.assertEqual(updates[-1] - updates[-2], 22)
        self.assertGreater(updates[-2] - updates[-3], 100)

    def test_quad_guard_timing_and_update_pulses(self):
        values = rate.frequencies(100e6, 101e6, 100)
        for pre, post, pulse in ((2, 6, 2), (0, 6, 2), (0, 3, 2), (0, 1, 2), (0, 0, 2), (0, 0, 1)):
            with self.subTest(pre=pre, post=post, pulse=pulse):
                waveform = rate.Ad9959Waveform(100e6, "streamer", 4, pre, post, pulse)
                waveform.sweep(values)
                frames, updates, pulses = decode_forwarded(waveform.runs, 3.0, serial_bits=4)
                self.assertEqual(len(frames), 100)
                self.assertEqual(pulses, [pulse] * 100)
                self.assertEqual(sum(ticks for ticks, pins in waveform.runs if not pins & 2), 1000)
                self.assertEqual(set(b - a for a, b in zip(updates, updates[1:])), {10 + pre + post + 2 * pulse})
                for frequency, frame in zip(values, frames):
                    self.assertEqual(len(frame), 5)
                    self.assertEqual(frame[0], 4)
                    realized = int.from_bytes(frame[1:], "big") * 500e6 / 2**32
                    self.assertLessEqual(abs(realized - frequency), 500e6 / 2**33)

    def test_quad_invalid_timing_is_rejected(self):
        for kwargs in ({"serial_bits": 2}, {"pre_guard_ticks": -1}, {"post_guard_ticks": -1},
                       {"update_ticks": 0}, {"update_ticks": 1.5}):
            with self.assertRaises(ValueError):
                rate.Ad9959Waveform(100e6, "streamer", **kwargs)
        with self.assertRaises(ValueError):
            rate.Ad9959Waveform(50e6, "qout", 4)

    def test_quad_shuffled_long_prefix_cli_and_setup_timing(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "run"
            argv = ["spi_update_rate.py", "--generate-only", "--mode", "endpoints", "--points", "10",
                    "--clock-source", "streamer", "--serial-bits", "4", "--burst-cycles", "3",
                    "--shuffle-seed", "42", "--pre-guard-ticks", "0", "--post-guard-ticks", "0",
                    "--update-ticks", "1", "--run-dir", str(directory)]
            with patch("sys.argv", argv), patch.object(rate, "Board", side_effect=AssertionError("hardware accessed")), \
                    contextlib.redirect_stdout(io.StringIO()):
                rate.main()
            report = json.loads((directory / "report.json").read_text())
            self.assertEqual(report["serial_bits"], 4)
            self.assertEqual(report["burst_updates"], 30)
            self.assertEqual(report["update_interval_s"], 12 / 100e6)
            self.assertEqual([info["updates"] for info in report["sequences"]], list(range(21, 31)))
            self.assertCountEqual(report["frequency_order_Hz"], rate.frequencies(100e6, 101e6, 10))
            self.assertNotEqual(report["frequency_order_Hz"], sorted(report["frequency_order_Hz"]))
            setup, _ = expand_sequence((directory / "setup.seq").read_text())
            self.assertEqual(decode_forwarded(setup, 3.0)[2], [100, 2, 2])
            for info in report["sequences"]:
                runs, _ = expand_sequence((directory / info["file"]).read_text())
                frames, _, _ = decode_forwarded(runs, 3.0, serial_bits=4)
                self.assertEqual(len(frames), info["updates"])
                realized = int.from_bytes(frames[-1][1:], "big") * 500e6 / 2**32
                self.assertLessEqual(abs(realized - info["endpoint_Hz"]), 500e6 / 2**33)

    def test_no_hardware_generation_has_complete_endpoint_prefixes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "run"
            argv = ["spi_update_rate.py", "--generate-only", "--mode", "endpoints", "--points", "3",
                    "--run-dir", str(directory)]
            with patch("sys.argv", argv), patch.object(rate, "Board", side_effect=AssertionError("hardware accessed")), \
                    contextlib.redirect_stdout(io.StringIO()):
                rate.main()
            report = json.loads((directory / "report.json").read_text())
            self.assertEqual(report["status"], "generated")
            self.assertEqual([item["updates"] for item in report["sequences"]], [1, 2, 3])
            self.assertEqual(report["sequences"][-1]["endpoint_Hz"], 101e6)
            self.assertEqual(report["measurements"], [])

    def test_bad_digital_evidence_is_rejected_and_raw_overflow_is_explicit(self):
        with tempfile.TemporaryDirectory() as temporary:
            board = rate.Board("unused", Path(temporary))
            board.directory = "/home/root/spi-rate.test"
            info = dict(file="test.seq", crc32=0x1234, ticks=41202)
            good = ("DMA_RESULT valid=1 expected_crc=0x1234 output_crc=0x1234 "
                    "readback_crc=0x1234 final=0x2 ticks=41202 capture_overflow=1")
            for status, text, accepted in ((0, good, True), (1, good, False), (0, "", False),
                                           (0, good.replace("output_crc=0x1234", "output_crc=0x1235"), False),
                                           (0, good.replace("ticks=41202", "ticks=41203"), False)):
                with patch.object(rate.subprocess, "run", return_value=subprocess.CompletedProcess([], status, text)):
                    if accepted:
                        self.assertTrue(board.play_checked(info, Path(temporary) / "test.log"))
                    else:
                        with self.assertRaises(RuntimeError):
                            board.play_checked(info, Path(temporary) / "test.log")

    def test_forwarded_cli_generation_records_clock_source_and_crc_scope(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "run"
            argv = ["spi_update_rate.py", "--generate-only", "--mode", "endpoints", "--points", "3",
                    "--clock-source", "streamer", "--run-dir", str(directory)]
            with patch("sys.argv", argv), patch.object(rate, "Board", side_effect=AssertionError("hardware accessed")), \
                    contextlib.redirect_stdout(io.StringIO()):
                rate.main()
            report = json.loads((directory / "report.json").read_text())
            self.assertEqual(report["status"], "generated")
            self.assertEqual(report["spi_clock_source"], "streamer")
            self.assertEqual(report["spi_actual_Hz"], 100e6)
            self.assertEqual(report["update_interval_s"], 52 / 100e6)
            for count, info in enumerate(report["sequences"], 1):
                runs, _ = expand_sequence((directory / info["file"]).read_text())
                self.assertEqual(len(decode_forwarded(runs, 3.0)[0]), count)
            self.assertIn("Forwarded SCLK is outside", (directory / "report.md").read_text())

    def test_running_sweep_blocks_new_test(self):
        with patch.object(rate, "run_command", return_value="python3 -u tools/spi_payload/sweep2.py --output sweep2.dat\n"):
            with self.assertRaisesRegex(RuntimeError, "still running"):
                rate.ensure_idle()


if __name__ == "__main__":
    unittest.main()
