#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Upload and stream a small named-channel timeline through ppscpi."""

import argparse

from pulsepins import PulsePins, Timeline


def build_timeline(clock_hz):
    timeline = Timeline(unit="us", clock_hz=clock_hz)
    timeline.channel("laser", bit=0)
    timeline.channel("camera", bit=1)
    timeline.channel("gate", bit=2)
    timeline.pulse("gate", start=0, duration=40)
    timeline.pulse("laser", start=10, duration=5)
    timeline.pulse("camera", start=20, duration=10)
    return timeline


def main():
    parser = argparse.ArgumentParser(
        description="Upload and stream a Timeline through ppscpi."
    )
    parser.add_argument(
        "host",
        nargs="?",
        default="de10nano",
        help="board hostname or IP address running ppscpi (default: de10nano)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=5025,
        help="ppscpi TCP port (default: 5025)",
    )
    parser.add_argument(
        "--clock-hz",
        type=float,
        default=100_000_000,
        help="streamer clock used for unit conversion (default: 100 MHz)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="enable ppscpi readback checking before streaming",
    )
    parser.add_argument(
        "--print-sequence",
        action="store_true",
        help="print the generated sequence before upload",
    )
    args = parser.parse_args()

    timeline = build_timeline(args.clock_hz)
    if args.print_sequence:
        print(timeline.to_sequence(force_trigger=True), end="")

    with PulsePins(args.host, port=args.port) as pp:
        print(pp.idn())
        pp.reset()
        pp.check(args.check)
        pp.load(timeline, force_trigger=True)
        print(pp.stream())


if __name__ == "__main__":
    main()
