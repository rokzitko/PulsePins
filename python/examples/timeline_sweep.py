#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Run a simple host-side delay sweep through ppscpi."""

import argparse

from pulsepins import PulsePins, Timeline


def build_timeline(clock_hz, camera_delay_us):
    laser_start_us = 10
    laser_duration_us = 5
    camera_duration_us = 10
    end_us = max(
        laser_start_us + laser_duration_us,
        laser_start_us + camera_delay_us + camera_duration_us,
    )

    timeline = Timeline(unit="us", clock_hz=clock_hz)
    timeline.channel("laser", bit=0)
    timeline.channel("camera", bit=1)
    timeline.channel("gate", bit=2)
    timeline.pulse("gate", start=0, duration=end_us + 5)
    timeline.pulse("laser", start=laser_start_us, duration=laser_duration_us)
    timeline.pulse(
        "camera",
        start=laser_start_us + camera_delay_us,
        duration=camera_duration_us,
    )
    return timeline


def main():
    parser = argparse.ArgumentParser(
        description="Sweep camera delay relative to a laser pulse through ppscpi."
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
        "--delays-us",
        type=float,
        nargs="+",
        default=[0, 5, 10, 20],
        help="camera delays after laser start, in microseconds",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="enable ppscpi readback checking before streaming",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print generated sequences without connecting to ppscpi",
    )
    args = parser.parse_args()

    timelines = [
        (delay, build_timeline(args.clock_hz, delay)) for delay in args.delays_us
    ]

    if args.dry_run:
        for delay, timeline in timelines:
            print("# camera delay: {} us".format(delay))
            print(timeline.to_sequence(force_trigger=True), end="")
        return

    with PulsePins(args.host, port=args.port) as pp:
        print(pp.idn())
        pp.reset()
        for delay, timeline in timelines:
            print("camera delay: {} us".format(delay))
            print(pp.run(timeline, check=args.check, force_trigger=True))


if __name__ == "__main__":
    main()
