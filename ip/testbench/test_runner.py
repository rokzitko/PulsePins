"""Integration tests for runner isolation and verdicts using small real RTL fixtures."""

from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


RUNNER = Path(__file__).with_name("run.py").resolve()
PROBE = """
`timescale 1ns/1ps
module tb;
  integer fh;
  initial begin
    #1;
    repeat (8) $display("DATA=%08h", $urandom);
    $display("SCENARIO sampled_words 8 8");
    fh = $fopen("SUCCESS", "w");
    $fclose(fh);
    $finish;
  end
endmodule
"""


@unittest.skipUnless(shutil.which("iverilog") and shutil.which("vvp"), "Icarus is required")
class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pulsepins runner ")
        self.addCleanup(self.temp.cleanup)
        self.bench = Path(self.temp.name)
        self.source = self.bench / "tb.sv"
        self.source.write_text(PROBE)
        self.script = self.bench / "run1"
        self.script.write_text("#!/bin/sh\nexec " + shlex.join([sys.executable, str(RUNNER)]) +
                               ' "$0" tb tb.sv "$@"\n')

    def run_bench(self, seed="42", sim="icarus", waves="0", verbose="0", extra=(), **env):
        environment = dict(os.environ, SIM=sim, SEED=seed, WAVES=waves,
                           VERBOSE=verbose, TIMEOUT_SEC="30")
        environment.update(env)
        result = subprocess.run([sys.executable, str(RUNNER), str(self.script),
                                 "tb", "tb.sv", *extra], env=environment,
                                text=True, capture_output=True, timeout=40)
        output = re.search(r" output=(.+)", result.stdout)
        self.assertIsNotNone(output, result.stdout + result.stderr)
        directory = Path(output.group(1))
        metadata = json.loads((directory / "run.json").read_text())
        return result, directory, metadata

    def assert_pass(self, run):
        result, directory, metadata = run
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(metadata["status"], "passed")
        return directory

    def data(self, directory):
        values = re.findall(r"DATA=([0-9a-fA-F]+)", (directory / "run.log").read_text())
        self.assertEqual(len(values), 8)
        return values

    def test_parallel_invocations_and_seed_replay(self):
        with ThreadPoolExecutor(max_workers=3) as pool:
            runs = list(pool.map(self.run_bench, ["42", "42", "43"]))
        directories = [self.assert_pass(run) for run in runs]
        self.assertEqual(len(set(directories)), 3)
        self.assertEqual(self.data(directories[0]), self.data(directories[1]))
        self.assertNotEqual(self.data(directories[0]), self.data(directories[2]))
        self.assertFalse((self.bench / "SUCCESS").exists())
        for directory in directories:
            self.assertTrue((directory / "SUCCESS").is_file())

    def test_random_seed_is_recorded_and_replayable(self):
        first = self.run_bench(seed="random")
        self.assert_pass(first)
        seed = first[2]["seed"]
        self.assertIn(f"SEED={seed}", first[2]["replay"])
        replay = subprocess.run(first[2]["replay"], shell=True, text=True,
                                capture_output=True, timeout=40)
        self.assertEqual(replay.returncode, 0, replay.stdout + replay.stderr)
        self.assertEqual(self.data(first[1]), re.findall(r"DATA=([0-9a-fA-F]+)", replay.stdout))

    def test_compile_failure_cannot_reuse_stale_marker(self):
        previous = self.assert_pass(self.run_bench())
        (self.bench / "SUCCESS").touch()
        self.source.write_text("module tb; broken syntax !!! endmodule\n")
        result, directory, metadata = self.run_bench()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(metadata["status"], "failed")
        self.assertFalse((directory / "SUCCESS").exists())
        self.assertTrue((previous / "SUCCESS").exists())

    def test_replay_preserves_compile_defines(self):
        self.source.write_text(PROBE.replace("#1;", '''#1;
`ifndef REPLAY_DEFINE
    $fatal(1, "replay lost the compile define");
`endif
'''))
        first = self.run_bench(extra=("--define", "REPLAY_DEFINE"))
        self.assert_pass(first)
        replay = subprocess.run(first[2]["replay"], shell=True, text=True,
                                capture_output=True, timeout=40)
        self.assertEqual(replay.returncode, 0, replay.stdout + replay.stderr)

    def test_fatal_after_marker_is_a_failure(self):
        self.source.write_text(PROBE.replace("$finish;", '$fatal(1, "injected failure");'))
        result, directory, metadata = self.run_bench()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(metadata["status"], "failed")
        self.assertTrue((directory / "SUCCESS").exists())
        self.assertIn("injected failure", result.stderr)

    def test_missing_marker_requires_explicit_legacy_mode(self):
        self.source.write_text('module tb; initial begin #1; $display("SCENARIO completed 1 1"); $finish; end endmodule\n')
        (self.bench / "SUCCESS").touch()
        result, _, metadata = self.run_bench()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(metadata["status"], "failed")
        self.assertIn("without a SUCCESS marker", result.stderr)
        self.assert_pass(self.run_bench(extra=("--no-marker",)))

    def test_waves_are_opt_in(self):
        quiet = self.assert_pass(self.run_bench())
        waves = self.assert_pass(self.run_bench(waves="1"))
        self.assertFalse(list(quiet.glob("*.vcd")))
        self.assertGreater((waves / "waves.vcd").stat().st_size, 0)

    def test_scenario_counts_are_recorded(self):
        run = self.run_bench()
        self.assert_pass(run)
        self.assertEqual(run[2]["scenario_coverage"]["sampled_words"],
                         {"observed": 8, "required": 8, "met": True})
        self.assertIn("[COVERAGE]", run[0].stdout)

    def test_missing_scenario_summary_cannot_pass_with_marker(self):
        self.source.write_text(PROBE.replace('$display("SCENARIO sampled_words 8 8");', ''))
        result, directory, metadata = self.run_bench()
        self.assertTrue((directory / "SUCCESS").exists())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("without required scenario coverage", result.stderr)
        self.assertEqual(metadata["status"], "failed")

    def test_unmet_or_duplicate_scenario_cannot_pass(self):
        for report in ('SCENARIO sampled_words 0 8', 'SCENARIO sampled_words 8 0'):
            with self.subTest(report=report):
                self.source.write_text(PROBE.replace('SCENARIO sampled_words 8 8', report))
                result, _, metadata = self.run_bench()
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(metadata["scenario_coverage"]["sampled_words"]["met"])
        self.source.write_text(PROBE.replace('$finish;', '$display("SCENARIO sampled_words 8 8"); $finish;'))
        result, _, _ = self.run_bench()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate scenario report", result.stderr)

    def test_verbose_trace_is_opt_in_and_quiet_log_is_bounded(self):
        self.source.write_text(PROBE.replace("#1;", '#1; repeat (1000) $display("TRACE");'))
        quiet = self.run_bench()
        loud = self.run_bench(verbose="1")
        self.assert_pass(quiet)
        self.assert_pass(loud)
        self.assertNotIn("TRACE", quiet[0].stdout)
        self.assertEqual(loud[0].stdout.count("TRACE"), 1000)
        self.assertLess((quiet[1] / "run.log").read_text().count("TRACE"), 100)
        self.assertEqual((loud[1] / "run.log").read_text().count("TRACE"), 1000)

    def test_early_nonfatal_error_survives_log_truncation(self):
        self.source.write_text(PROBE.replace("#1;", '#1; $error("early failure"); repeat (1000) $display("TRACE");'))
        result, directory, metadata = self.run_bench()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(metadata["status"], "failed")
        self.assertIn("early failure", (directory / "run.log").read_text())
        self.assertIn("early failure", result.stderr)

    def test_wall_clock_watchdog_bounds_silent_hang(self):
        self.source.write_text('module tb; initial begin $display("waiting forever"); forever #1; end endmodule\n')
        result, _, metadata = self.run_bench(TIMEOUT_SEC="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(metadata["status"], "failed")
        self.assertIn("wall-clock timeout", result.stderr)
        self.assertIn("waiting forever", result.stderr)

    @unittest.skipUnless(shutil.which("vsim") and shutil.which("vlog"), "Questa is optional")
    def test_questa_seed_replay_and_wave_capture(self):
        runs = [self.run_bench(seed=seed, sim="questa") for seed in ("42", "42", "43")]
        for run in runs:
            self.assert_pass(run)
        self.assertEqual(self.data(runs[0][1]), self.data(runs[1][1]))
        self.assertNotEqual(self.data(runs[0][1]), self.data(runs[2][1]))
        self.assertFalse(list(runs[0][1].glob("*.wlf")))
        wave_run = self.assert_pass(self.run_bench(sim="questa", waves="1"))
        self.assertGreater((wave_run / "waves.wlf").stat().st_size, 0)


if __name__ == "__main__":
    unittest.main()
