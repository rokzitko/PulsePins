#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Build a small named-channel timeline without touching hardware."""

import argparse

from pulsepins import Timeline


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
        description="Print a Timeline-generated PulsePins text sequence."
    )
    parser.add_argument(
        "--clock-hz",
        type=float,
        default=100_000_000,
        help="streamer clock used for unit conversion (default: 100 MHz)",
    )
    parser.add_argument(
        "--svg",
        metavar="PATH",
        help="optional path for an SVG timeline preview",
    )
    parser.add_argument(
        "--csv",
        metavar="PATH",
        help="optional path for a browser-compatible Timeline CSV file",
    )
    parser.add_argument(
        "--draft",
        metavar="PATH",
        help="optional path for a browser-compatible Timeline draft JSON file",
    )
    args = parser.parse_args()

    timeline = build_timeline(args.clock_hz)
    print(timeline.to_sequence(force_trigger=True), end="")

    if args.svg:
        with open(args.svg, "w", encoding="utf-8") as output:
            output.write(timeline.to_svg())
    if args.csv:
        with open(args.csv, "w", encoding="utf-8") as output:
            output.write(timeline.to_csv())
    if args.draft:
        with open(args.draft, "w", encoding="utf-8") as output:
            output.write(timeline.to_draft_json())


if __name__ == "__main__":
    main()
