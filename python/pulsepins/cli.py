# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Console entry points for host-side PulsePins examples."""

import argparse
from pathlib import Path

from .scpi import PulsePins
from .timeline import Timeline


def build_example_timeline(clock_hz):
    timeline = Timeline(unit="us", clock_hz=clock_hz)
    timeline.channel("laser", bit=0)
    timeline.channel("camera", bit=1)
    timeline.channel("gate", bit=2)
    timeline.pulse("gate", start=0, duration=40)
    timeline.pulse("laser", start=10, duration=5)
    timeline.pulse("camera", start=20, duration=10)
    return timeline


def build_sweep_timeline(clock_hz, camera_delay_us):
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


def ppscpi_hello_main(argv=None):
    parser = argparse.ArgumentParser(
        description="Upload and stream a short sequence through ppscpi."
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
    args = parser.parse_args(argv)

    with PulsePins(args.host, port=args.port) as pp:
        print(pp.idn())
        pp.reset()
        pp.load_sequence("""
        d 10 0xff
        d 5 0x00
        f
        """)
        pp.stream()


def ppscpi_check_main(argv=None):
    parser = argparse.ArgumentParser(
        description="Check connectivity to a board running ppscpi."
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
        "--self-test",
        action="store_true",
        help="also run ppscpi TEST1 after the connectivity check",
    )
    args = parser.parse_args(argv)

    with PulsePins(args.host, port=args.port) as pp:
        print(pp.idn())
        print("STREAMER_CLOCK_HZ {:.17g}".format(pp.streamer_clock_hz()))
        print("CHECK {}".format("ON" if pp.check_enabled() else "OFF"))
        errors = pp.errors()
        if errors:
            for error in errors:
                print("ERROR {}".format(error))
        else:
            print("No SCPI errors")
        if args.self_test:
            print("TEST1 {}".format(pp.test1()))


def timeline_preview_main(argv=None):
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
    parser.add_argument(
        "--vcd",
        metavar="PATH",
        help="optional path for a scalar VCD timeline preview",
    )
    parser.add_argument(
        "--vcd-timescale",
        default="10ns",
        help="VCD timescale for --vcd output (default: 10ns)",
    )
    args = parser.parse_args(argv)

    timeline = build_example_timeline(args.clock_hz)
    print(timeline.to_sequence(force_trigger=True, include_final=True), end="")

    if args.svg:
        with open(args.svg, "w", encoding="utf-8") as output:
            output.write(timeline.to_svg())
    if args.csv:
        with open(args.csv, "w", encoding="utf-8") as output:
            output.write(timeline.to_csv())
    if args.draft:
        with open(args.draft, "w", encoding="utf-8") as output:
            output.write(timeline.to_draft_json())
    if args.vcd:
        with open(args.vcd, "w", encoding="utf-8") as output:
            output.write(timeline.to_vcd(timescale=args.vcd_timescale))


def timeline_stream_main(argv=None):
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
    args = parser.parse_args(argv)

    timeline = build_example_timeline(args.clock_hz)
    if args.print_sequence:
        print(timeline.to_sequence(force_trigger=True, include_final=True), end="")

    with PulsePins(args.host, port=args.port) as pp:
        print(pp.idn())
        pp.reset()
        pp.check(args.check)
        pp.load(timeline, force_trigger=True, include_final=True)
        print(pp.stream())


def timeline_sweep_main(argv=None):
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
    args = parser.parse_args(argv)

    timelines = [
        (delay, build_sweep_timeline(args.clock_hz, delay)) for delay in args.delays_us
    ]

    if args.dry_run:
        for delay, timeline in timelines:
            print("# camera delay: {} us".format(delay))
            print(timeline.to_sequence(force_trigger=True, include_final=True), end="")
        return

    with PulsePins(args.host, port=args.port) as pp:
        print(pp.idn())
        pp.reset()
        for delay, timeline in timelines:
            print("camera delay: {} us".format(delay))
            print(pp.run(timeline, check=args.check, force_trigger=True, include_final=True))


def notebook_workflow_main(argv=None):
    parser = argparse.ArgumentParser(
        description="Notebook-style host workflow: install, connect, preview, sweep, stream."
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
        help="dry-run streamer clock used for unit conversion (default: 100 MHz)",
    )
    parser.add_argument(
        "--delays-us",
        type=float,
        nargs="+",
        default=[0, 5, 10],
        help="camera delays after laser start, in microseconds",
    )
    parser.add_argument(
        "--output-dir",
        metavar="PATH",
        help="optional directory for SVG, CSV, draft JSON, and VCD previews",
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="connect to ppscpi and stream the example plus sweep",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="enable ppscpi readback checking while streaming with --run",
    )
    args = parser.parse_args(argv)

    print("# Install once from a checkout:")
    print("python3 -m pip install -e /path/to/PulsePins/python")

    def write_previews(timeline):
        if not args.output_dir:
            return
        output_dir = Path(args.output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        (output_dir / "timeline.svg").write_text(timeline.to_svg(), encoding="utf-8")
        (output_dir / "timeline.csv").write_text(timeline.to_csv(), encoding="utf-8")
        (output_dir / "timeline.json").write_text(
            timeline.to_draft_json(), encoding="utf-8"
        )
        (output_dir / "timeline.vcd").write_text(
            timeline.to_vcd(timescale="10ns"), encoding="utf-8"
        )
        print("# Wrote previews to {}".format(output_dir))

    def show_sequence(timeline):
        print("# Generated sequence:")
        print(timeline.to_sequence(force_trigger=True, include_final=True), end="")

    if not args.run:
        print("# Dry run using clock_hz={:.17g}".format(args.clock_hz))
        timeline = build_example_timeline(args.clock_hz)
        write_previews(timeline)
        show_sequence(timeline)
        for delay in args.delays_us:
            print("# Sweep camera delay: {} us".format(delay))
            print(
                build_sweep_timeline(args.clock_hz, delay).to_sequence(
                    force_trigger=True,
                    include_final=True,
                ),
                end="",
            )
        return

    with PulsePins(args.host, port=args.port) as pp:
        print("# Connected:")
        print(pp.idn())
        clock_hz = pp.streamer_clock_hz()
        print("# Board streamer clock: {:.17g} Hz".format(clock_hz))
        timeline = build_example_timeline(clock_hz)
        write_previews(timeline)
        show_sequence(timeline)
        pp.reset()
        print("# Stream example:")
        print(pp.run(timeline, check=args.check, force_trigger=True, include_final=True))
        for delay in args.delays_us:
            print("# Stream sweep camera delay: {} us".format(delay))
            print(
                pp.run(
                    build_sweep_timeline(clock_hz, delay),
                    check=args.check,
                    force_trigger=True,
                    include_final=True,
                )
            )
