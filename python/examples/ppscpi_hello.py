#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Minimal host-side PulsePins SCPI example."""

import argparse

from pulsepins import PulsePins


def main():
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
    args = parser.parse_args()

    with PulsePins(args.host, port=args.port) as pp:
        print(pp.idn())
        pp.reset()
        pp.load_sequence("""
        d 10 0xff
        d 5 0x00
        f
        """)
        pp.stream()


if __name__ == "__main__":
    main()
