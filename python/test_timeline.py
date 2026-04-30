# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

import pytest

from pulsepins import Timeline, TimelineError


def test_timeline_compiles_overlapping_channels():
    tl = Timeline()
    tl.channel("laser", 0)
    tl.channel("camera", 1)
    tl.pulse("laser", start=10, duration=5)
    tl.pulse("camera", start=12, duration=4)

    assert tl.records() == [
        (10, 0x0),
        (2, 0x1),
        (3, 0x3),
        (1, 0x2),
    ]
    assert tl.to_sequence() == "d 10 0x0\nd 2 0x1\nd 3 0x3\nd 1 0x2\n"


def test_timeline_allows_adjacent_pulses_on_same_channel():
    tl = Timeline()
    tl.channel("q0", 0)
    tl.pulse("q0", 0, 5)
    tl.pulse("q0", 5, 5)

    assert tl.records() == [(10, 0x1)]


def test_timeline_rejects_same_channel_overlap():
    tl = Timeline()
    tl.channel("q0", 0)
    tl.pulse("q0", 0, 5)
    tl.pulse("q0", 4, 1)

    with pytest.raises(TimelineError, match="overlapping pulses"):
        tl.to_sequence()


def test_timeline_converts_absolute_units_to_cycles():
    tl = Timeline(unit="us", clock_hz=100_000_000)
    tl.channel("q0", 0)
    tl.pulse("q0", start=0.1, duration=0.05)

    assert tl.pulses == (("q0", 10, 5),)
    assert tl.to_sequence(force_trigger=True) == "d 10 0x0\nd 5 0x1\nf\n"


def test_timeline_rejects_non_integer_cycle_conversion():
    tl = Timeline(unit="ns", clock_hz=100_000_000)
    tl.channel("q0", 0)

    with pytest.raises(TimelineError, match="not an integer number of cycles"):
        tl.pulse("q0", start=1, duration=10)


def test_timeline_preserves_unowned_initial_bits():
    tl = Timeline(initial_value=0x8)
    tl.channel("q0", 0)
    tl.pulse("q0", 2, 3)

    assert tl.to_sequence(include_final=True) == "d 2 0x8\nd 3 0x9\nfinal 0x8\n"


def test_timeline_renders_svg_preview():
    tl = Timeline()
    tl.channel("q0", 0)
    tl.pulse("q0", 0, 5)

    svg = tl.to_svg(width=320)
    assert svg.startswith('<svg xmlns="http://www.w3.org/2000/svg"')
    assert "q0[0]" in svg
    assert "5 cycles" in svg
