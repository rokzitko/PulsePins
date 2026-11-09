"""Acquisition ordering, pacing and result provenance without hardware."""

import contextlib
import io
import math
import unittest
from unittest.mock import patch

import sweep1


class SweepTest(unittest.TestCase):
    def test_endpoints_and_amplitude_law(self):
        points = sweep1.sweep_points(100)
        self.assertEqual(len(points), 100)
        self.assertEqual(points[0], (0, 1023))
        self.assertEqual(points[-1], (200e6, 102))
        for (earlier, _), (later, _) in zip(points, points[1:]):
            self.assertAlmostEqual(later - earlier, 200e6 / 99)
        for frequency, amplitude in points:
            self.assertLessEqual(abs(amplitude / 1023 - (1 - 0.9 * frequency / 200e6)), 0.5 / 1023 + 1e-15)
        with self.assertRaises(ValueError):
            sweep1.sweep_points(1)

    def test_measure_before_advancing_and_preserve_measured_values(self):
        clock = [0.0]
        events = []
        points = sweep1.sweep_points(3)

        class Board:
            def play(self, filename):
                events.append(("set", filename, clock[0]))
                clock[0] += 0.1

        class Analyzer:
            def peak(self, frequency):
                events.append(("measure", frequency, clock[0]))
                # The second measurement overruns the nominal 600 ms point interval.
                clock[0] += 0.9 if frequency == 100e6 else 0.2
                return (frequency - 1234 if frequency else math.nan), -20

        def sleep(seconds):
            self.assertGreaterEqual(seconds, 0)
            clock[0] += seconds

        output = io.StringIO()
        with patch.object(sweep1.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(sweep1.time, "sleep", side_effect=sleep), contextlib.redirect_stdout(io.StringIO()):
            elapsed, valid = sweep1.acquire(Board(), Analyzer(), points, 1.8, output)
        self.assertEqual([event[0] for event in events], ["set", "measure"] * 3)
        self.assertAlmostEqual(events[2][2], 0.6)
        self.assertAlmostEqual(events[4][2], 1.6)  # waited for the slow measurement
        self.assertAlmostEqual(elapsed, 2.2)  # no accelerated catch-up after the overrun
        self.assertEqual(valid, 2)
        rows = [line.split() for line in output.getvalue().splitlines() if not line.startswith("#")]
        self.assertEqual(len(rows), 3)
        self.assertTrue(math.isnan(float(rows[0][1])))
        self.assertEqual(tuple(map(float, rows[-1])), (200e6, 200e6 - 1234))

    def test_failed_playback_does_not_generate_a_measurement(self):
        class Board:
            def play(self, filename):
                raise RuntimeError("readback mismatch")

        class Analyzer:
            def peak(self, frequency):
                raise AssertionError("Must not measure after a failed DDS update")

        output = io.StringIO()
        with self.assertRaisesRegex(RuntimeError, "readback mismatch"):
            sweep1.acquire(Board(), Analyzer(), sweep1.sweep_points(2), 1.2, output)
        self.assertEqual(output.getvalue(), "")

    def test_peak_waits_for_fresh_trace(self):
        class Analyzer(sweep1.Siglent):
            def __init__(self, measured, level):
                super().__init__("unused")
                self.commands = []
                self.measured = measured
                self.level = level

            def write(self, command):
                self.commands.append(command)

            def query(self, command):
                self.write(command)
                return {"*OPC?": "1", ":FREQ:CENT?": "100000000", "*ESR?": "0", ":SWE:TIME?": "0.035",
                        ":CALC:MARK1:X?": str(self.measured), ":CALC:MARK1:Y?": str(self.level)}[command]

        analyzer = Analyzer(99998500, -30)
        with patch.object(sweep1.time, "sleep"):
            self.assertEqual(analyzer.peak(100e6), (99998500, -30))
        commands = analyzer.commands
        start = commands.index(":INIT:IMM")
        self.assertLess(start, commands.index("*OPC?", start))
        self.assertLess(commands.index("*OPC?", start), commands.index(":CALC:MARK1:MAX"))
        self.assertLess(commands.index(":CALC:MARK1:MAX"), commands.index(":CALC:MARK1:X?"))
        for measured, level in ((99998500, -95), (98e6, -20), (math.nan, -20)):
            with patch.object(sweep1.time, "sleep"):
                self.assertTrue(math.isnan(Analyzer(measured, level).peak(100e6)[0]))

        class LateMarker(Analyzer):
            def query(self, command):
                if command == ":CALC:MARK1:X?":
                    self.measured = 0 if self.commands.count(":INIT:IMM") == 1 else 99998500
                return super().query(command)

        analyzer = LateMarker(0, -30)
        with patch.object(sweep1.time, "sleep") as sleep:
            self.assertEqual(analyzer.peak(100e6), (99998500, -30))
        self.assertEqual(analyzer.commands.count(":INIT:IMM"), 2)
        self.assertEqual(sleep.call_count, 2)
        self.assertTrue(all(call.args[0] >= 0.035 for call in sleep.call_args_list))


if __name__ == "__main__":
    unittest.main()
