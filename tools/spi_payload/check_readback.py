"""Compare ppplay -read output with the generator's d/store/r/final/f subset."""

from pathlib import Path
from functools import lru_cache
import re
import sys
import zlib


def expand_sequence(text):
    runs = []
    slots = {}
    final = None
    for line in text.splitlines():
        fields = line.split()
        if not fields or fields == ["f"]:
            continue
        if final is not None:
            raise ValueError("Record after final")
        if fields[0] == "d" and len(fields) == 3:
            count, pins = (int(value, 0) for value in fields[1:])
            runs.append((count, pins))
        elif fields[0] == "store" and len(fields) == 5 and fields[2] == "d":
            slot, count, pins = (int(fields[index], 0) for index in (1, 3, 4))
            if not 0 <= slot < 8:
                raise ValueError("Store slot outside 0..7")
            slots[slot] = (count, pins)
        elif fields[0] == "r" and len(fields) == 3:
            repetitions, length = (int(value, 0) for value in fields[1:])
            if repetitions <= 0 or not 1 <= length <= 8:
                raise ValueError("Expected finite replay of 1..8 slots")
            runs.extend([slots[slot] for slot in range(length)] * repetitions)
        elif fields[0] == "final" and len(fields) == 2:
            final = int(fields[1], 0)
        else:
            raise ValueError("Unsupported sequence record: " + line)
    if final is None:
        raise ValueError("Expected explicit final output")
    if any(not 0 < count <= 0xFFFFFFFF or not 0 <= pins <= 0xFFFFFFFF for count, pins in runs):
        raise ValueError("Invalid data record")
    return runs, final


def merge_runs(runs):
    merged = []
    for count, pins in runs:
        if merged and merged[-1][1] == pins:
            merged[-1] = (merged[-1][0] + count, pins)
        else:
            merged.append((count, pins))
    return merged


def waveform_crc32(runs):
    # Match the FPGA's IEEE CRC-32: one little-endian 32-bit pin word per tick.
    # Bound allocation even for multi-second holds, caching common sample runs.
    @lru_cache(maxsize=32)
    def block(pins, count):
        return pins.to_bytes(4, "little") * count

    crc = 0
    for count, pins in runs:
        while count > 65536:
            crc = zlib.crc32(block(pins, 65536), crc)
            count -= 65536
        crc = zlib.crc32(block(pins, count), crc)
    return crc


def check_readback(sequence_text, log, summary_only=False):
    runs, final = expand_sequence(sequence_text)
    expected = merge_runs(runs)
    if summary_only:
        report = re.search(r"Readback report: size=(\d+) length=(\d+)", log)
        ticks = sum(count for count, _ in expected)
        if report is None or (int(report[1]), int(report[2])) != (len(expected), ticks):
            raise ValueError("Readback summary does not match expected run count and duration")
        crc_match = re.search(r"send_and_trig\(\): CRC=0x([0-9a-fA-F]+).*OK", log)
        if crc_match is None or int(crc_match[1], 16) != waveform_crc32(expected):
            raise ValueError("Hardware CRC does not match the intended pin waveform")
        if not re.search(r"All done, exiting with return code 0\b", log):
            raise ValueError("Playback did not report successful completion")
        final_match = re.search(r"Final qout=0x([0-9a-fA-F]+)", log)
        if final_match is None or int(final_match[1], 16) != final:
            raise ValueError("Missing or mismatched final qout")
        return len(expected), ticks
    pattern = r"^C (\d+) value=0x([0-9a-fA-F]+)\s+.*?\bcount=0x([0-9a-fA-F]+)\b"
    captured = []
    for match in re.finditer(pattern, log, re.MULTILINE):
        if int(match[1]) != len(captured):
            raise ValueError("Readback index discontinuity")
        captured.append((int(match[3], 16), int(match[2], 16)))
    if len(captured) != len(expected):
        raise ValueError(f"Run count mismatch: captured {len(captured)}, expected {len(expected)}")
    for index, (actual, wanted) in enumerate(zip(captured, expected)):
        if actual != wanted:
            raise ValueError(f"Run {index}: captured {actual}, expected {wanted} (ticks, pins)")
    final_match = re.search(r"Final qout=0x([0-9a-fA-F]+)", log)
    if final_match is None or int(final_match[1], 16) != final:
        raise ValueError("Missing or mismatched final qout")
    return len(captured), sum(count for count, _ in captured)


if __name__ == "__main__":
    summary_only = len(sys.argv) > 1 and sys.argv[1] == "--summary"
    arguments = sys.argv[2:] if summary_only else sys.argv[1:]
    if len(arguments) != 2:
        sys.exit("Usage: python3 check_readback.py [--summary] sequence.seq ppplay-read.log")
    try:
        count, ticks = check_readback(Path(arguments[0]).read_text(), Path(arguments[1]).read_text(), summary_only)
        label = "Readback summary and waveform CRC match" if summary_only else "Readback matches"
        print(f"{label}: {count} runs, {ticks} ticks; final qout matches.")
    except (OSError, ValueError, KeyError) as error:
        sys.exit(str(error))
