"""Prove trigger regressions reject representative RTL defects in isolated copies."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


STREAMER = Path(__file__).resolve().parents[1]
IP = STREAMER.parent
RUNNER = IP / "testbench/run.py"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--primitive-only", action="store_true")
    parser.add_argument("--sim", choices=("icarus", "questa"), default="questa")
    args = parser.parse_args()
    if args.sim == "icarus" and not args.primitive_only:
        parser.error("chain/wrapper tests require Questa vendor libraries; use --primitive-only for Icarus")
    primitive = (STREAMER / "tb_and_trigger/tb2.sv", STREAMER / "and_trigger.sv")
    chain = (STREAMER / "tb_chain_trigger/tb2.sv", STREAMER / "and_trigger.sv", STREAMER / "chain_trigger.sv")
    wrapper = (STREAMER / "tb_st_interface/tb10.sv", *sorted(STREAMER.glob("*.sv")),
               IP / "misc/endianness.sv", IP / "misc/crc32.sv", IP / "misc/sync.sv", IP / "misc/cdc.sv")
    groups = {"primitive": ("tb_and_exhaustive", primitive), "chain": ("tb_chain_async", chain),
              "wrapper": ("tb_st_if_triggers", wrapper)}
    predicate = "((i ~^ pattern) & mask) == mask"
    mutations = [
        ("primitive", "ignore_enable", "and_trigger.sv", "if (trigger_enable)", "if (1'b1)"),
        ("primitive", "ignore_mask", "and_trigger.sv", predicate, "i == pattern"),
        ("primitive", "any_matching_bit", "and_trigger.sv", predicate, "((i ~^ pattern) & mask) != 0"),
        ("primitive", "lose_high_bit", "and_trigger.sv", predicate, "((i ~^ pattern) & mask & 8'h7f) == (mask & 8'h7f)"),
        ("primitive", "lose_latch", "and_trigger.sv", "o <= 1'b1;", "o <= 1'b1; else o <= 1'b0;"),
        ("primitive", "reset_priority", "and_trigger.sv", "if (reset)", "if (reset && !trigger_enable)"),
        ("chain", "retain_on_retrig", "chain_trigger.sv",
         "else if (retrig) begin\n    state <= S_IDLE;\n    active_stage_valid <= 1'b0;",
         "else if (retrig) begin\n    state <= S_IDLE;\n    active_stage_valid <= 1'b1;"),
        ("chain", "discard_on_trigger_reset", "chain_trigger.sv",
         "if (trigger_reset)\n      state <= S_IDLE;",
         "if (trigger_reset) begin state <= S_IDLE; active_stage_valid <= 0; end"),
        ("chain", "pop_during_load_reset", "chain_trigger.sv", "rdreq = !(rst || retrig);", "rdreq = 1'b1;"),
        ("chain", "premature_final", "chain_trigger.sv", "else if (is_last || fifo_empty)", "else if (1'b1)"),
        ("chain", "ignore_final", "chain_trigger.sv", "else if (is_last || fifo_empty)", "else if (fifo_empty)"),
        ("chain", "force_over_reset", "chain_trigger.sv", "if (trigger_reset)", "if (trigger_reset && !trigger_force)"),
        ("wrapper", "lose_external_enable", "st_interface.sv", ".async_in(trigger_enable_ext)", ".async_in(1'b0)"),
        ("wrapper", "force_and_instead_of_or", "st_interface.sv",
         "trigger_force_int_streamer  | trigger_force_ext_streamer", "trigger_force_int_streamer & trigger_force_ext_streamer"),
        ("wrapper", "reset_and_instead_of_or", "st_interface.sv",
         "trigger_reset_int_streamer  | trigger_reset_ext_streamer", "trigger_reset_int_streamer & trigger_reset_ext_streamer"),
        ("wrapper", "lose_external_high_bit", "st_interface.sv", ".async_in(trigger_in[trigger_sync_i])",
         ".async_in(trigger_sync_i == WIDTH_TRIGGER-1 ? 1'b0 : trigger_in[trigger_sync_i])"),
    ]
    if args.primitive_only:
        groups = {"primitive": groups["primitive"]}
        mutations = [mutation for mutation in mutations if mutation[0] == "primitive"]
    root = Path(__file__).parent / ".sim" / "mutations"
    root.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix="trigger-", dir=root))
    environment = dict(os.environ, SIM=args.sim, SEED="1", VERBOSE="0", WAVES="0")

    def run(group, name, replacement=None):
        top, originals = groups[group]
        sources = [replacement if replacement and path.name == replacement.name else path for path in originals]
        command = [sys.executable, str(RUNNER), str(artifacts / name), top, *map(str, sources)]
        if group != "primitive":
            command += ["--library", "altera_mf", "--library", "lpm"]
        result = subprocess.run(command, env=environment, text=True, capture_output=True, timeout=240)
        (artifacts / f"{name}.log").write_text(result.stdout + result.stderr)
        return result

    for group in groups:
        result = run(group, f"baseline_{group}")
        if result.returncode:
            raise RuntimeError(f"baseline {group} failed:\n{result.stdout}{result.stderr}")
        print(f"PASS baseline {group}", flush=True)
    for group, name, filename, old, new in mutations:
        original = (STREAMER / filename).read_text()
        if original.count(old) != 1:
            raise RuntimeError(f"mutation {name} needs review: anchor does not occur exactly once")
        directory = artifacts / name
        directory.mkdir()
        mutant = directory / filename
        mutant.write_text(original.replace(old, new, 1))
        (directory / "config.vh").write_text((STREAMER / "config.vh").read_text())
        result = run(group, name, mutant)
        diagnostic = result.stdout + result.stderr
        # A compiler error is not evidence that a functional checker works.
        if not result.returncode or not ("** Fatal:" in diagnostic or "FATAL:" in diagnostic):
            raise RuntimeError(f"mutation {name} was not rejected by a functional check:\n{diagnostic}")
        print(f"PASS rejected {name}", flush=True)
    print(f"SUCCESS: {len(mutations)} trigger mutations rejected; artifacts={artifacts}")


if __name__ == "__main__":
    main()
