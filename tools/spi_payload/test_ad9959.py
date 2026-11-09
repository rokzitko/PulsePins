"""Decode the emitted pin waveform as an AD9959, independently of the encoder."""

import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zlib

from check_readback import check_readback, expand_sequence, waveform_crc32


GENERATOR = Path(sys.argv.pop(1)).resolve()


class Ad9959Test(unittest.TestCase):
    def generate(self, directory, *frequencies):
        path = Path(directory) / "tones.seq"
        result = subprocess.run(
            [str(GENERATOR), str(path), *map(str, frequencies)],
            capture_output=True, text=True,
        )
        return result, path

    def decode(self, text, update_count=2, allowed_pins=0xC7):
        lines = text.splitlines()
        self.assertEqual(lines[-2:], ["final 0x2", "f"])
        previous = 2
        now = 0
        last_data_change = 0
        last_clock_edge = 0
        selected_at = None
        bits = []
        transactions = []
        updates = []
        reset_start = None
        reset_end = None
        update_start = None
        for line in lines[:-2]:
            record, count, value = line.split()
            self.assertEqual(record, "d")
            count, pins = int(count), int(value, 0)
            self.assertGreater(count, 0)
            self.assertLessEqual(count, 0xFFFFFFFF)
            self.assertEqual(pins & ~allowed_pins, 0)  # unused lines stay low
            if pins & 2:
                self.assertEqual(pins & 1, 0)  # idle SCLK low
            if pins & 0xC0:
                self.assertTrue(pins & 2)  # reset/UPDATE outside transactions
            if pins & 0x80 and not previous & 0x80:
                self.assertIsNone(reset_start)
                reset_start = now
            if previous & 0x80 and not pins & 0x80:
                self.assertGreaterEqual(now - reset_start, 100)
                reset_end = now
            if pins & 0x40 and not previous & 0x40:
                update_start = now
                updates.append(now)
            if previous & 0x40 and not pins & 0x40:
                self.assertGreaterEqual(now - update_start, 100)
            if previous & 2 and not pins & 2:
                self.assertIsNotNone(reset_end)
                self.assertGreaterEqual(now - reset_end, 100)
                selected_at = now
                bits = []
            if (pins ^ previous) & 4:
                last_data_change = now
            if pins & 1 and not previous & 1:
                self.assertIsNotNone(selected_at)
                self.assertGreaterEqual(now - selected_at, 1)
                self.assertGreaterEqual(now - last_data_change, 1)
                self.assertGreaterEqual(now - last_clock_edge, 4)
                bits.append((pins >> 2) & 1)
            if previous & 1 and not pins & 1:
                self.assertGreaterEqual(now - last_clock_edge, 4)
            if (previous ^ pins) & 1:
                last_clock_edge = now
            if not previous & 2 and pins & 2:
                self.assertTrue(bits)
                self.assertEqual(len(bits) % 8, 0)
                data = bytes(
                    sum(bit << (7 - j) for j, bit in enumerate(bits[i:i + 8]))
                    for i in range(0, len(bits), 8)
                )
                transactions.append((now, data))
                selected_at = None
            previous = pins
            now += count
        self.assertIsNotNone(reset_end)
        self.assertIsNone(selected_at)
        self.assertEqual(previous, 2)
        self.assertEqual(len(updates), update_count)
        return transactions, updates

    def check_program(self, frequencies, expected_words=None):
        with tempfile.TemporaryDirectory() as directory:
            result, path = self.generate(directory, *frequencies)
            self.assertEqual(result.returncode, 0, result.stderr)
            transactions, updates = self.decode(path.read_text())

        # Interpret channel selection and register writes, rather than comparing
        # a copy of the encoder's token stream. CSR does not disable RF outputs.
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3}
        selected = 0xF0
        channels = [{} for _ in range(4)]
        for index, (time, data) in enumerate(transactions):
            address = data[0]
            self.assertIn(address, widths)  # also excludes read instructions
            self.assertEqual(len(data), widths[address] + 1)
            value = int.from_bytes(data[1:], "big")
            if index == 0:
                self.assertEqual((address, value), (1, 0xD00000))
                self.assertLess(time, updates[0])
            else:
                self.assertGreaterEqual(time - updates[0], 200000)
                self.assertLess(time, updates[1])
            if address == 0:
                self.assertEqual(value & 0xF, 0)  # remain in reset-default serial mode
                selected = value
            elif address != 1:
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value

        self.assertEqual(selected, 0xF0)
        if not frequencies:
            frequencies = (50e6, 75e6, 100e6, 125e6)
        for channel, registers in enumerate(channels):
            self.assertEqual(registers[3], 0x301)  # sine, full DAC current, running
            self.assertEqual(registers[5], 0)
            self.assertEqual(registers[6], 0x13FF)
            if expected_words:
                self.assertEqual(registers[4], expected_words[channel])
            self.assertLessEqual(abs(registers[4] * 500e6 / 2**32 - frequencies[channel]),
                                 500e6 / 2**33)

    def test_default_tones(self):
        self.check_program((), (0x1999999A, 0x26666666, 0x33333333, 0x40000000))

    def test_custom_tones(self):
        self.check_program((1, 10e6, 80e6, 200e6))

    def test_frequency_staircase(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test1.seq"
            result = subprocess.run([str(GENERATOR), "--test1", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        # ABI 5 finite playback can queue at most 3382 elements, including final.
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        transactions, updates = self.decode(text, update_count=27)
        selected = 0xF0
        words = [0] * 4
        snapshots = []
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append(tuple(words))
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            if time > updates[1]:
                self.assertIn(address, (0, 4))
                self.assertEqual(len(data), 2 if address == 0 else 5)
                if address == 4:
                    self.assertEqual(selected, 0x20)
            if address == 0:
                selected = value
            elif address == 4:
                for channel in range(4):
                    if selected & (0x10 << channel):
                        words[channel] = value
        expected = (0x1999999A, 0x26666666, 0x33333333, 0x40000000)
        self.assertEqual([state[1] for state in snapshots[2:]], list(expected) * 6 + [expected[1]])
        for state in snapshots[1:]:
            self.assertEqual((state[0], state[2], state[3]), (expected[0], expected[2], expected[3]))
        for earlier, later in zip(updates[2:], updates[3:]):
            self.assertGreaterEqual(later - earlier, 100000000)
            self.assertLess(later - earlier, 100002000)
        self.assertEqual(tuple(words), expected)
        self.assertEqual(selected, 0xF0)

    def test_two_level_fsk(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test2.seq"
            result = subprocess.run([str(GENERATOR), "--test2", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        transactions, updates = self.decode(text, update_count=4, allowed_pins=0x2C7)
        first_fsk_write = next(time for time, _ in transactions if time > updates[1])
        self.assertGreaterEqual(first_fsk_write - updates[1], 1000000)
        selected = 0xF0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3, 10: 4}
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append([registers.copy() for registers in channels])
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                self.assertEqual(value, 0xD00000)  # two-level mode, RU/RD disabled
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        self.assertEqual(snapshots[2][1][3], 0x800301)
        self.assertEqual(snapshots[2][1][4], 0x23D70A3D)  # 70 MHz, P1 low
        self.assertEqual(snapshots[2][1][10], 0x28F5C28F)  # 80 MHz, P1 high
        self.assertEqual(snapshots[3][1][3], 0x301)
        self.assertEqual(snapshots[3][1][4], 0x26666666)  # return to 75 MHz
        for snapshot in snapshots[2:]:
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][channel])

        # Decode the complete RF selection interval: only P1 changes, at 500 ms
        # intervals, with no SPI traffic, reset or UPDATE during the 24 cycles.
        now = 0
        holds = []
        for line in text.splitlines()[:-2]:
            _, count, pins = line.split()
            count, pins = int(count), int(pins, 0)
            if count >= 50000000:
                self.assertEqual(count, 50000000)
                self.assertGreater(now, updates[2])
                self.assertLess(now + count, updates[3])
                holds.append((now, pins))
            now += count
        self.assertEqual([pins for _, pins in holds], [2, 0x202] * 24)
        self.assertEqual(holds[-1][0] + 50000000 - holds[0][0], 2400000000)
        self.assertFalse(any(holds[0][0] <= time < holds[-1][0] + 50000000
                             for time, _ in transactions))
        self.assertEqual(selected, 0xF0)

    def test_sixteen_level_fsk(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test3.seq"
            result = subprocess.run([str(GENERATOR), "--test3", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        transactions, updates = self.decode(text, update_count=4, allowed_pins=0xFC7)
        selected = 0xF0
        fr1 = 0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3}
        widths.update({address: 4 for address in range(0x0A, 0x19)})
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append((fr1, [registers.copy() for registers in channels]))
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                fr1 = value
                self.assertEqual((fr1 >> 18) & 31, 20)
                self.assertTrue(fr1 & (1 << 23))
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        mode, programmed = snapshots[2]
        self.assertEqual((mode >> 12) & 7, 1)  # all four profile pins assigned to CH1
        self.assertEqual((mode >> 8) & 3, 3)  # 16 levels
        self.assertEqual((mode >> 10) & 3, 0)  # no RU/RD
        self.assertEqual(programmed[1][3], 0x800301)
        words = [programmed[1][4]] + [programmed[1][address] for address in range(0x0A, 0x19)]
        for profile, word in enumerate(words):
            self.assertLessEqual(abs(word * 500e6 / 2**32 - (60e6 + 2e6 * profile)), 500e6 / 2**33)
        self.assertEqual(snapshots[3][0], 0xD00000)
        self.assertEqual(snapshots[3][1][1][3], 0x301)
        self.assertEqual(snapshots[3][1][1][4], 0x26666666)
        for _, snapshot in snapshots[2:]:
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][1][channel])

        now = 0
        holds = []
        burst = 0
        for line in text.splitlines()[:-2]:
            _, count, pins = line.split()
            count, pins = int(count), int(pins, 0)
            # Bound dense bursts between pauses so initialization cannot fill
            # the board's readback FIFO faster than the host can drain it.
            burst = 0 if count >= 100000 else burst + 1
            self.assertLessEqual(burst, 800)
            if count >= 25000000:
                self.assertEqual(count, 25000000)
                self.assertEqual(pins & ~0xF00, 2)
                self.assertGreater(now, updates[2])
                self.assertLess(now + count, updates[3])
                # Datasheet [P0:P3]: Q8 is the MSB, Q11 the LSB.
                profile = ((pins >> 8) & 1) * 8 + ((pins >> 9) & 1) * 4
                profile += ((pins >> 10) & 1) * 2 + ((pins >> 11) & 1)
                holds.append((now, 60 + 2 * profile))
            now += count
        expected = [60, 74, 64, 86, 70, 80, 62, 90, 76, 66, 84, 72, 88, 68, 82, 78]
        self.assertEqual([frequency for _, frequency in holds], expected * 6)
        self.assertEqual(holds[-1][0] + 25000000 - holds[0][0], 2400000000)
        self.assertFalse(any(holds[0][0] <= time < holds[-1][0] + 25000000
                             for time, _ in transactions))
        self.assertEqual(selected, 0xF0)

    def test_asymmetric_sweep(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test4.seq"
            result = subprocess.run([str(GENERATOR), "--test4", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        transactions, updates = self.decode(text, update_count=4, allowed_pins=0x2C7)
        selected = 0xF0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3, 7: 2, 8: 4, 9: 4, 10: 4}
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append([registers.copy() for registers in channels])
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                self.assertEqual(value, 0xD00000)  # sweep requires two-level mode
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        sweep = snapshots[2][1]
        self.assertEqual(sweep[3], 0x806301)
        self.assertEqual(sweep[4], 0x1EB851EC)
        self.assertEqual(sweep[10], 0x2E147AE1)
        excursion = sweep[10] - sweep[4]
        up_step_s = (sweep[7] & 255) / 125e6
        down_step_s = (sweep[7] >> 8) / 125e6
        up_duration = math.ceil(excursion / sweep[8]) * up_step_s
        down_duration = math.ceil(excursion / sweep[9]) * down_step_s
        self.assertGreater(up_duration, 0.99)
        self.assertLessEqual(up_duration, 1.0)
        self.assertGreater(down_duration, 2.99)
        self.assertLessEqual(down_duration, 3.0)
        self.assertAlmostEqual((sweep[8] / up_step_s) / (sweep[9] / down_step_s), 3.0)
        self.assertEqual(snapshots[3][1][3], 0x301)
        self.assertEqual(snapshots[3][1][4], 0x26666666)
        for snapshot in snapshots[2:]:
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][channel])

        now = 0
        holds = []
        burst = 0
        for line in text.splitlines()[:-2]:
            _, count, pins = line.split()
            count, pins = int(count), int(pins, 0)
            burst = 0 if count >= 100000 else burst + 1
            self.assertLessEqual(burst, 800)
            if count >= 100000000:
                self.assertGreater(now, updates[2])
                self.assertLess(now + count, updates[3])
                holds.append((now, count, pins))
            now += count
        self.assertEqual([pins for _, _, pins in holds], [0x202, 2] * 6)
        for _, count, pins in holds:
            target = 100000000 if pins == 0x202 else 300000000
            self.assertGreaterEqual(count, target)
            self.assertLess(count, target + 100)
        self.assertFalse(any(holds[0][0] <= time < holds[-1][0] + holds[-1][1]
                             for time, _ in transactions))
        self.assertEqual(selected, 0xF0)

    def test_no_dwell_sweep(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test5.seq"
            result = subprocess.run([str(GENERATOR), "--test5", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        transactions, updates = self.decode(text, update_count=4, allowed_pins=0x2C7)
        selected = 0xF0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3, 7: 2, 8: 4, 10: 4}
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append([registers.copy() for registers in channels])
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                self.assertEqual(value, 0xD00000)  # PLL x20, two-level mode
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        sweep = snapshots[2][1]
        self.assertEqual((sweep[3] >> 22) & 3, 2)  # frequency
        self.assertTrue(sweep[3] & (1 << 15))  # no dwell at upper endpoint
        self.assertTrue(sweep[3] & (1 << 14))  # linear sweep enabled
        self.assertTrue(sweep[3] & (1 << 13))  # load timer on UPDATE
        self.assertEqual(sweep[4], 0x1EB851EC)  # 60 MHz
        self.assertEqual(sweep[10], 0x2E147AE1)  # 90 MHz
        step_s = (sweep[7] & 255) / 125e6
        duration = math.ceil((sweep[10] - sweep[4]) / sweep[8]) * step_s
        self.assertGreater(duration, 0.49)
        self.assertLessEqual(duration, 0.5)
        self.assertEqual(snapshots[3][1][3], 0x301)
        self.assertEqual(snapshots[3][1][4], 0x26666666)
        for snapshot in snapshots[2:]:
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][channel])

        # Decode every retrigger edge, checking that P1 remains high beyond
        # the automatic return and then goes low before the next sweep.
        now = 0
        holds = []
        burst = 0
        for line in text.splitlines()[:-2]:
            _, count, pins = line.split()
            count, pins = int(count), int(pins, 0)
            burst = 0 if count >= 100000 else burst + 1
            self.assertLessEqual(burst, 800)
            if count >= 25000000:
                self.assertGreater(now, updates[2])
                self.assertLess(now + count, updates[3])
                holds.append((now, count, pins))
            now += count
        self.assertEqual([pins for _, _, pins in holds], [0x202, 2] * 24)
        rising_edges = []
        for time, count, pins in holds:
            if pins == 0x202:
                self.assertGreater(count / 100e6, duration + 0.2)
                rising_edges.append(time)
            else:
                self.assertGreaterEqual(count, 25000000)
        self.assertEqual([b - a for a, b in zip(rising_edges, rising_edges[1:])], [100000000] * 23)
        elapsed = holds[-1][0] + holds[-1][1] - holds[0][0]
        self.assertGreaterEqual(elapsed, 2400000000)
        self.assertLess(elapsed, 2400000100)
        self.assertFalse(any(holds[0][0] <= time < holds[-1][0] + holds[-1][1]
                             for time, _ in transactions))
        self.assertEqual(selected, 0xF0)

    def test_amplitude_staircase(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test6.seq"
            result = subprocess.run([str(GENERATOR), "--test6", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        transactions, updates = self.decode(text, update_count=28)
        selected = 0xF0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3}
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append([registers.copy() for registers in channels])
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                self.assertEqual(value, 0xD00000)
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                    self.assertEqual(address, 6)  # only digital amplitude changes
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        for snapshot, target_db in zip(snapshots[2:-1], [0, -6, -12, -18, -24] * 5):
            amplitude = snapshot[1][6]
            self.assertEqual(amplitude & ~0x3FF, 0x1000)  # manual scaling enabled
            relative_db = 20 * math.log10((amplitude & 0x3FF) / 1023)
            self.assertAlmostEqual(relative_db, target_db, delta=0.1)
        for snapshot in snapshots[2:]:
            self.assertEqual(snapshot[1][4], 0x26666666)  # 75 MHz throughout
            self.assertEqual(snapshot[1][3], 0x301)  # full DAC current, single tone
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][channel])
        self.assertEqual(snapshots[-1], snapshots[1])  # full initial state restored
        for earlier, later in zip(updates[2:], updates[3:]):
            self.assertGreaterEqual(later - earlier, 100000000)
            self.assertLess(later - earlier, 100002000)
        self.assertGreaterEqual(updates[-1] - updates[2], 2500000000)
        burst = 0
        for line in text.splitlines()[:-2]:
            _, count, _ = line.split()
            burst = 0 if int(count) >= 100000 else burst + 1
            self.assertLessEqual(burst, 800)
        self.assertEqual(selected, 0xF0)

    def test_sampled_sinusoidal_am(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test7.seq"
            result = subprocess.run([str(GENERATOR), "--test7", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        self.assertEqual(sum(line.startswith("store ") for line in text.splitlines()), 8)
        self.assertEqual([line for line in text.splitlines() if line.startswith("r ")], ["r 24000 8"])
        runs, final = expand_sequence(text)
        self.assertEqual(final, 2)
        expanded = "".join(f"d {count} {pins:#x}\n" for count, pins in runs) + "final 0x2\nf\n"
        transactions, updates = self.decode(expanded, update_count=4, allowed_pins=0x7C7)
        selected = 0xF0
        fr1 = 0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3}
        widths.update({address: 4 for address in range(0x0A, 0x11)})
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append((fr1, [registers.copy() for registers in channels]))
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                fr1 = value
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        mode, programmed = snapshots[2]
        self.assertEqual(mode, 0xD01200)  # eight-level mode, P0:P2 assigned to CH1
        self.assertEqual(programmed[1][3], 0x400301)  # amplitude, not frequency modulation
        self.assertEqual(programmed[1][6] & ~0x3FF, 0x1000)  # multiplier on, no automatic ramp
        scales = [programmed[1][6] & 0x3FF]
        scales += [programmed[1][address] >> 22 for address in range(0x0A, 0x11)]
        self.assertEqual(snapshots[3][0], 0xD00000)
        self.assertEqual(snapshots[3][1][1][3], 0x301)
        self.assertEqual(snapshots[3][1][1][6], 0x13FF)
        for _, snapshot in snapshots[2:]:
            self.assertEqual(snapshot[1][4], 0x26666666)  # 75 MHz throughout
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][1][channel])

        now = 0
        samples = []
        burst = 0
        for count, pins in runs:
            burst = 0 if count >= 12500 else burst + 1
            self.assertLessEqual(burst, 100)
            if count == 12500:
                self.assertEqual(pins & ~0x700, 2)  # only profiles change during AM
                self.assertGreater(now, updates[2])
                self.assertLess(now + count, updates[3])
                profile = ((pins >> 8) & 1) * 4 + ((pins >> 9) & 1) * 2 + ((pins >> 10) & 1)
                samples.append((now, scales[profile]))
            now += count
        self.assertEqual(len(samples), 192000)
        self.assertEqual(samples[-1][0] + 12500 - samples[0][0], 2400000000)
        self.assertFalse(any(samples[0][0] <= time < samples[-1][0] + 12500
                             for time, _ in transactions))
        period = [scale for _, scale in samples[:8]]
        for index, scale in enumerate(period):
            self.assertAlmostEqual(scale, 682 * (1 + 0.5 * math.cos(2 * math.pi * index / 8)), delta=0.5)
        self.assertAlmostEqual((max(period) - min(period)) / (max(period) + min(period)), 0.5)
        self.assertEqual(samples[8][0] - samples[0][0], 100000)  # 1 kHz at 100 MHz
        # Predict the RF sidebands from the sample DFT and zero-order hold.
        real = sum(scale * math.cos(2 * math.pi * index / 8) for index, scale in enumerate(period))
        imag = sum(scale * math.sin(2 * math.pi * index / 8) for index, scale in enumerate(period))
        hold_response = math.sin(math.pi / 8) / (math.pi / 8)
        sideband_db = 20 * math.log10(math.hypot(real, imag) / sum(period) * hold_response)
        self.assertAlmostEqual(sideband_db, -12.3, delta=0.1)
        self.assertEqual(selected, 0xF0)

    def test_native_phase_keying(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test8.seq"
            result = subprocess.run([str(GENERATOR), "--test8", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        self.assertEqual(sum(line.startswith("store ") for line in text.splitlines()), 2)
        self.assertEqual([line for line in text.splitlines() if line.startswith("r ")], ["r 30000 2"] * 4)
        runs, final = expand_sequence(text)
        self.assertEqual(final, 2)
        expanded = "".join(f"d {count} {pins:#x}\n" for count, pins in runs) + "final 0x2\nf\n"
        transactions, updates = self.decode(expanded, update_count=4, allowed_pins=0x2C7)
        selected = 0xF0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3, 10: 4}
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append([registers.copy() for registers in channels])
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                self.assertEqual(value, 0xD00000)  # two-level mode; P1 selects CH1
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                    self.assertIn(address, (3, 5, 10))
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        psk = snapshots[2][1]
        self.assertEqual(psk[3], 0xC00301)  # phase modulation, no sweep, full DAC current
        phases = [psk[5] & 0x3FFF, psk[10] >> 18]
        self.assertEqual([phase * 360 / 2**14 for phase in phases], [0, 180])
        # Equal-duration opposite phases cancel the carrier's complex mean.
        real = sum(math.cos(2 * math.pi * phase / 2**14) for phase in phases) / 2
        imag = sum(math.sin(2 * math.pi * phase / 2**14) for phase in phases) / 2
        self.assertLess(math.hypot(real, imag), 1e-12)
        self.assertEqual(snapshots[3][1][3], 0x301)
        for snapshot in snapshots[2:]:
            self.assertEqual(snapshot[1][4], 0x26666666)  # 75 MHz
            self.assertEqual(snapshot[1][6], 0x13FF)  # constant full amplitude
            self.assertEqual(snapshot[1][5], 0)  # plain blocks select zero phase
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][channel])

        now = 0
        holds = []
        burst = 0
        for count, pins in runs:
            burst = 0 if count >= 5000 else burst + 1
            self.assertLessEqual(burst, 100)
            if count == 5000 or count >= 300000000:
                self.assertGreater(now, updates[2])
                self.assertLess(now + count, updates[3])
                holds.append((now, count, pins))
            now += count
        self.assertIn(f"{now} ticks", result.stdout)
        self.assertEqual(len(holds), 4 * (60000 + 1))
        for block in range(4):
            chunk = holds[block * 60001:(block + 1) * 60001]
            self.assertEqual([count for _, count, _ in chunk[:-1]], [5000] * 60000)
            self.assertEqual([pins for _, _, pins in chunk[:-1]], [2, 0x202] * 30000)
            self.assertEqual(chunk[2][0] - chunk[0][0], 10000)  # 10 kHz at 100 MHz
            self.assertEqual(chunk[-1][0] - chunk[0][0], 300000000)  # 3 s PSK
            self.assertEqual(chunk[-1][1:], (300000000, 2))  # 3 s plain carrier
        end = holds[-1][0] + holds[-1][1]
        self.assertEqual(end - holds[0][0], 2400000000)
        self.assertFalse(any(holds[0][0] <= time < end for time, _ in transactions))
        self.assertEqual(selected, 0xF0)

    def test_burst_edge_comparison(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test9.seq"
            result = subprocess.run([str(GENERATOR), "--test9", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        self.assertEqual([line for line in text.splitlines() if line.startswith("r ")], ["r 250 2"] * 4)
        runs, final = expand_sequence(text)
        self.assertEqual(final, 2)
        expanded = "".join(f"d {count} {pins:#x}\n" for count, pins in runs) + "final 0x2\nf\n"
        transactions, updates = self.decode(expanded, update_count=7, allowed_pins=0x2C7)
        selected = 0xF0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3, 10: 4}
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append([registers.copy() for registers in channels])
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                self.assertEqual(value, 0xD00000)  # two-level ASK or single-tone RU/RD
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                    self.assertIn(address, (3, 6, 10))
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value
        for block, snapshot in enumerate(snapshots[2:6]):
            ch1 = snapshot[1]
            if block % 2 == 0:
                self.assertEqual(ch1[3], 0x400301)  # direct amplitude modulation
                self.assertEqual(ch1[6], 0x1000)  # manual scale zero when P1 is low
                self.assertEqual(ch1[10] >> 22, 1023)  # full amplitude when P1 is high
            else:
                self.assertEqual(ch1[3], 0x301)  # single tone, profiles control auto RU/RD
                acr = ch1[6]
                self.assertEqual(acr & 0x1C00, 0x1C00)  # multiplier, auto ramp, timer reload
                maximum = acr & 0x3FF
                self.assertEqual(maximum, 1023)
                step = 1 << ((acr >> 14) & 3)
                ramp_s = math.ceil(maximum / step) * ((acr >> 16) & 255) / 125e6
                self.assertAlmostEqual(ramp_s, 0.0005, delta=0.00001)
        for snapshot in snapshots[2:]:
            self.assertEqual(snapshot[1][4], 0x26666666)  # fixed 75 MHz
            self.assertEqual(snapshot[1][5], 0)  # fixed phase
            self.assertEqual((snapshot[1][3] >> 8) & 3, 3)  # full DAC current
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][channel])
        self.assertEqual(snapshots[-1][1][3], 0x301)
        self.assertEqual(snapshots[-1][1][6], 0x13FF)  # restore manual full amplitude

        now = 0
        rising = []
        burst = 0
        for index, (count, pins) in enumerate(runs):
            burst = 0 if count >= 1000000 else burst + 1
            self.assertLessEqual(burst, 100)
            if pins & 0x200:
                self.assertEqual((count, pins), (1000000, 0x202))  # 10 ms on
                self.assertEqual(runs[index + 1], (1000000, 2))  # 10 ms off
                rising.append(now)
            now += count
        self.assertIn(f"{now} ticks", result.stdout)
        self.assertEqual(len(rising), 1000)
        for block in range(4):
            chunk = rising[block * 250:(block + 1) * 250]
            self.assertEqual([b - a for a, b in zip(chunk, chunk[1:])], [2000000] * 249)
            start, end = chunk[0], chunk[-1] + 2000000
            self.assertEqual(end - start, 500000000)  # exactly 5 s per burst block
            self.assertGreater(start, updates[block + 2])
            self.assertLess(end, updates[block + 3])
            self.assertFalse(any(start <= time < end for time, _ in transactions))
            self.assertLess(updates[block + 3] - end, 4000000)  # short programming gap
        self.assertEqual(selected, 0xF0)

    def test_dac_current_comparison(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test10.seq"
            result = subprocess.run([str(GENERATOR), "--test10", str(path)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = path.read_text()
        self.assertLessEqual(len(text.splitlines()) - 1, 3382)
        transactions, updates = self.decode(text, update_count=27)
        selected = 0xF0
        channels = [{} for _ in range(4)]
        snapshots = []
        widths = {0: 1, 1: 3, 3: 3, 4: 4, 5: 2, 6: 3}
        events = [(time, data) for time, data in transactions]
        events += [(time, None) for time in updates]
        for time, data in sorted(events, key=lambda event: event[0]):
            if data is None:
                snapshots.append([registers.copy() for registers in channels])
                continue
            address, value = data[0], int.from_bytes(data[1:], "big")
            self.assertIn(address, widths)
            self.assertEqual(len(data), widths[address] + 1)
            if address == 0:
                self.assertEqual(value & 0xF, 0)
                selected = value
            elif address == 1:
                self.assertEqual(value, 0xD00000)
            else:
                if time > updates[1]:
                    self.assertEqual(selected, 0x20)
                    self.assertIn(address, (3, 6))
                for channel in range(4):
                    if selected & (0x10 << channel):
                        channels[channel][address] = value

        # Datasheet Table 5: half and quarter are deliberately not in binary order.
        current_ratio = {3: 1.0, 1: 0.5, 2: 0.25, 0: 0.125}
        for index, snapshot in enumerate(snapshots[2:-1]):
            ch1 = snapshot[1]
            code = (ch1[3] >> 8) & 3
            scale = ch1[6] & 0x3FF
            self.assertEqual(ch1[3] & ~0x300, 1)  # single tone, sine, enabled DAC
            self.assertEqual(ch1[6] & ~0x3FF, 0x1000)  # manual digital scaling
            if index % 8 < 4:
                self.assertEqual(scale, 1023)  # DAC current is the only attenuator
            else:
                self.assertEqual(code, 3)  # digital comparison uses full DAC current
            relative_db = 20 * math.log10(current_ratio[code] * scale / 1023)
            self.assertAlmostEqual(relative_db, -6 * (index % 4), delta=0.1)
        for snapshot in snapshots[2:]:
            self.assertEqual(snapshot[1][4], 0x26666666)  # fixed 75 MHz
            self.assertEqual(snapshot[1][5], 0)
            for channel in (0, 2, 3):
                self.assertEqual(snapshot[channel], snapshots[1][channel])
        self.assertEqual(snapshots[-1], snapshots[1])  # full current and amplitude restored
        for earlier, later in zip(updates[2:], updates[3:]):
            self.assertGreaterEqual(later - earlier, 100000000)
            self.assertLess(later - earlier, 100002000)
        self.assertGreaterEqual(updates[-1] - updates[2], 2400000000)
        burst = 0
        for line in text.splitlines()[:-2]:
            _, count, _ = line.split()
            burst = 0 if int(count) >= 100000 else burst + 1
            self.assertLessEqual(burst, 800)
        self.assertEqual(selected, 0xF0)

    def test_replayed_readback_comparison(self):
        sequence = "d 2 0x2\nstore 0 d 3 0x2\nstore 1 d 4 0x402\nr 2 2\nfinal 0x2\nf\n"
        log = "\n".join([
            "C 0 value=0x00000002 [2] count=0x00000005 [5] (load) control=0x0",
            "C 1 value=0x00000402 [1026] count=0x00000004 [4] (load) control=0x0",
            "C 2 value=0x00000002 [2] count=0x00000003 [3] (load) control=0x0",
            "C 3 value=0x00000402 [1026] count=0x00000004 [4] (load) control=0x0",
            "send_and_trig(): Final qout=0x00000002 [2] not checked (expected value not inferred)",
        ])
        self.assertEqual(check_readback(sequence, log), (4, 16))
        for damaged in (log.replace("count=0x00000005", "count=0x00000006"),
                        log.replace("C 2 ", "missing "),
                        log.replace("Final qout=0x00000002", "Final qout=0x00000000")):
            with self.assertRaises(ValueError):
                check_readback(sequence, damaged)

    def test_quiet_readback_crc_comparison(self):
        # Independently serialize a stream that crosses the checker's chunk
        # boundary, including both repeated and changing pin values.
        data = bytes.fromhex("78563412") * 70001 + bytes(12) + bytes.fromhex("efbeedfe") * 11
        crc = zlib.crc32(data)
        self.assertEqual(waveform_crc32([(70001, 0x12345678), (3, 0), (11, 0xFEEDBEEF)]), crc)
        sequence = "d 70001 0x12345678\nd 3 0\nd 11 0xfeedbeef\nfinal 0x2\nf\n"
        log = (f"Readback report: size=3 length=70015\n"
               f"send_and_trig(): Final qout=0x00000002\n"
               f"send_and_trig(): CRC=0x{crc:08x} OK\n"
               f"All done, exiting with return code 0\n")
        self.assertEqual(check_readback(sequence, log, summary_only=True), (3, 70015))
        for damaged in (log.replace("size=3", "size=2"),
                        log.replace("length=70015", "length=70014"),
                        log.replace(f"{crc:08x}", "00000000"),
                        log.replace("return code 0", "return code 32"),
                        log.replace("qout=0x00000002", "qout=0x00000000")):
            with self.assertRaises(ValueError):
                check_readback(sequence, damaged, summary_only=True)

    def test_reject_invalid_frequencies_before_writing(self):
        for frequency in (0, -1, 200000001, math.nan, math.inf, "50MHz", 0.001):
            with self.subTest(frequency=frequency), tempfile.TemporaryDirectory() as directory:
                result, path = self.generate(directory, frequency, 75e6, 100e6, 125e6)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(path.exists())

    def test_reject_incomplete_channel_list(self):
        with tempfile.TemporaryDirectory() as directory:
            result, path = self.generate(directory, 50e6)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(path.exists())

    def test_help(self):
        result = subprocess.run([str(GENERATOR), "--help"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn("CH0_Hz CH1_Hz CH2_Hz CH3_Hz", result.stdout)

    def test_output_error(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([str(GENERATOR), directory], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
