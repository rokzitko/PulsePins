"""Decode the emitted pin waveform as an AD9959, independently of the encoder."""

import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


GENERATOR = Path(sys.argv.pop(1)).resolve()


class Ad9959Test(unittest.TestCase):
    def generate(self, directory, *frequencies):
        path = Path(directory) / "tones.seq"
        result = subprocess.run(
            [str(GENERATOR), str(path), *map(str, frequencies)],
            capture_output=True, text=True,
        )
        return result, path

    def decode(self, text):
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
            self.assertEqual(pins & ~0xC7, 0)  # unused SDIO, profiles, power-down low
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
        self.assertEqual(len(updates), 2)
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
