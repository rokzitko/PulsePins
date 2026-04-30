#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Run a simple host-side delay sweep through ppscpi."""

from pulsepins.cli import timeline_sweep_main


if __name__ == "__main__":
    timeline_sweep_main()
