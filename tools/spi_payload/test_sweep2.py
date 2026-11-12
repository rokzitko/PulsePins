"""Fixed analyzer range and valid fresh measurements throughout the full-scale sweep."""

import math
import unittest
from unittest.mock import patch

import sweep2


class Analyzer(sweep2.FixedSpanSiglent):
    def __init__(self, peaks):
        super().__init__("unused")
        self.commands = []
        self.peaks = peaks
        self.scans = 0
        self.stop = self.STOP_HZ

    def write(self, command):
        self.commands.append(command)
        if command == ":INIT:IMM":
            self.scans += 1

    def query(self, command):
        self.write(command)
        measured, level = self.peaks[min(max(self.scans - 1, 0), len(self.peaks) - 1)]
        return str({
            "*IDN?": "Siglent Technologies,SVA1015X,test", "*OPC?": 1, "*ESR?": 0,
            ":FREQ:STAR?": self.START_HZ, ":FREQ:STOP?": self.stop,
            ":BWID?": self.RBW_HZ, ":BWID:VID?": self.VBW_HZ,
            ":INIT:CONT?": 0, ":SWE:COUN?": 1, ":POW:ATT?": 20, ":POW:GAIN?": 0,
            ":SWE:TIME?": 0.035, ":CALC:MARK1:X?": measured, ":CALC:MARK1:Y?": level,
        }[command])


class SweepTest(unittest.TestCase):
    def test_full_range_at_full_amplitude(self):
        points = sweep2.sweep_points(100)
        self.assertEqual(len(points), 100)
        self.assertEqual(points[0], (0, 1023))
        self.assertEqual(points[-1], (200e6, 1023))
        self.assertTrue(all(amplitude == 1023 for _, amplitude in points))
        self.assertTrue(all(a[0] < b[0] for a, b in zip(points, points[1:])))
        with self.assertRaises(ValueError):
            sweep2.sweep_points(1)

    def test_range_is_set_once_and_never_tracks_points_or_retries(self):
        analyzer = Analyzer([(0, 0), (999e5, -12), (1999e5, -15)])
        analyzer.configure()
        configured = len(analyzer.commands)
        with patch.object(sweep2.time, "sleep") as sleep:
            self.assertEqual(analyzer.peak(100e6), (999e5, -12))
            self.assertEqual(analyzer.peak(200e6), (1999e5, -15))
        frequency_writes = [command for command in analyzer.commands
                            if command.startswith(":FREQ:") and "?" not in command]
        self.assertEqual(frequency_writes, [":FREQ:STAR 0", ":FREQ:STOP 250000000"])
        self.assertEqual(analyzer.scans, 3)
        self.assertEqual(sleep.call_count, 3)
        self.assertTrue(all(call.args[0] >= 0.035 for call in sleep.call_args_list))
        commands = analyzer.commands[configured:]
        start = commands.index(":INIT:IMM")
        self.assertLess(start, commands.index("*OPC?", start))
        self.assertLess(commands.index("*OPC?", start), commands.index(":CALC:MARK1:MAX"))
        self.assertLess(commands.index(":CALC:MARK1:MAX"), commands.index(":CALC:MARK1:X?"))

    def test_unrelated_spurs_stale_peaks_and_invalid_readings_are_rejected(self):
        for peak in ((0, 0), (50e6, -10), (98e6, -10), (100e6, -90),
                     (math.nan, -10), (100e6, math.nan)):
            analyzer = Analyzer([peak])
            with self.subTest(peak=peak), patch.object(sweep2.time, "sleep"):
                self.assertTrue(math.isnan(analyzer.peak(100e6)[0]))
                self.assertEqual(analyzer.scans, 3)
        with patch.object(sweep2.time, "sleep"):
            self.assertTrue(math.isnan(Analyzer([(0, -20)]).peak(0)[0]))

    def test_changed_or_invalid_range_aborts_acquisition(self):
        for stop in (200e6, math.nan):
            analyzer = Analyzer([(100e6, -10)])
            analyzer.stop = stop
            with self.subTest(stop=stop), self.assertRaisesRegex(RuntimeError, "range changed"):
                analyzer.peak(100e6)
            self.assertEqual(analyzer.scans, 0)


if __name__ == "__main__":
    unittest.main()
