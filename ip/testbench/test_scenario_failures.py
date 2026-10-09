"""Healthy DUTs must fail when stimulus omits a required coverage scenario."""

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


IP = Path(__file__).resolve().parents[1]
RUNNER = IP / "testbench/run.py"


class ScenarioFailures(unittest.TestCase):
    def check_omission(self, bench, script, top, source, dependencies, edits, missing,
                       prerequisites, sim="icarus", defines=(), libraries=(), no_marker=False):
        directory = IP / bench
        original = (directory / source).resolve()
        with tempfile.TemporaryDirectory(prefix="pulsepins-coverage-") as temporary:
            modified = Path(temporary) / original.name
            text = original.read_text()
            for old, new in edits:
                self.assertEqual(text.count(old), 1, f"omission fixture needs updating: {old}")
                text = text.replace(old, new, 1)
            modified.write_text(text)
            command = [sys.executable, str(RUNNER), str(directory / script), top,
                       *dependencies, str(modified)]
            for define in defines:
                command += ["--define", define]
            for library in libraries:
                command += ["--library", library]
            if no_marker:
                command.append("--no-marker")
            result = subprocess.run(command, env=dict(os.environ, SIM=sim, SEED="1", VERBOSE="0", WAVES="0"),
                                    text=True, capture_output=True, timeout=240)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(f"missing required scenario {missing}:", result.stderr,
                          result.stdout + result.stderr)
            match = re.search(r" output=(.+)", result.stdout)
            self.assertIsNotNone(match)
            output = Path(match.group(1))
            metadata = json.loads((output / "run.json").read_text())
            self.assertEqual(metadata["status"], "failed")
            self.assertFalse(metadata["scenario_coverage"][missing]["met"])
            for name in prerequisites:
                self.assertTrue(metadata["scenario_coverage"][name]["met"], name)
            self.assertFalse((output / "SUCCESS").exists())

    @unittest.skipUnless(shutil.which("iverilog"), "Icarus required")
    def test_constant_input_does_not_cover_random_correlation(self):
        self.check_omission("counter/tb_autocorrelation", "compile2", "hello_world", "tb2.sv",
            ["../autocorrelation.sv", "../../misc/cdc.sv"],
            [("rand_bit <= ($urandom_range(0, 100) > 30 ? $urandom_range(0, 1) : rand_bit);", "rand_bit <= 1'b0;")],
            "first_input_high", ["valid_samples", "checked_lag_addresses"], no_marker=True)

    @unittest.skipUnless(shutil.which("iverilog"), "Icarus required")
    def test_idle_reset_does_not_cover_reset_in_flight(self):
        self.check_omission("streamer/tb_streamer", "run14", "tb_decoder_reference", "tb14.sv",
            ["../rl_decoder.sv", "../prng.sv"],
            [("wait(received >= old_received + 5);", "wait(received == reference_count);")],
            "reset_active_run", ["output_mode_0", "output_mode_10", "prng_outputs_checked"])

    @unittest.skipUnless(shutil.which("vsim"), "Questa/vendor models required")
    def test_fifo_delivery_without_backpressure_does_not_pass(self):
        self.check_omission("streamer/tb_input_fifo", "run2", "tb2_input", "tb2.sv",
            ["../input_fifo.sv", "../preprocessor.sv"],
            [("wait(full === 1'b1);\n  repeat (4) @(negedge clk);", "#20;")],
            "write_backpressure", ["accepted_writes", "accepted_reads", "checked_words"],
            sim="questa", libraries=("altera_mf", "lpm"))

    @unittest.skipUnless(shutil.which("vsim"), "Questa required for CRC parameters")
    def test_continuous_crc_stream_does_not_cover_idle_gaps(self):
        self.check_omission("misc/tb_crc32", "run", "tb_crc32", "tb.sv", ["../crc32.sv"],
            [("send_word(32'hBAD0_DA7A, 1'b0);\n    send_word('0, 1'b0);", ""),
             ("send_word($urandom, 1'b0);", "")],
            "enabled_blocks_separated_by_gaps", ["crc_results_checked", "consecutive_enabled_cycles"], sim="questa")

    @unittest.skipUnless(shutil.which("vsim"), "Questa required")
    def test_programming_unused_force_bit_does_not_cover_force_path(self):
        self.check_omission("combiner_trig/tb_combiner", "run3", "tb_combiner_reference",
            "../../combiner/tb_combiner/tb3.sv", ["../combiner_trig.sv", "../../misc/cdc.sv"],
            [("for (int k = 0; k < 4; k++) begin\n      write_reg", "for (int k = 0; k < 4; k++) begin\n      if (k == 2) continue;\n      write_reg")],
            "force_path_3_checked", ["mode_0_output_checks", "mode_15_output_checks"],
            sim="questa", defines=("TRIGGER_COMBINER",))


if __name__ == "__main__":
    unittest.main()
