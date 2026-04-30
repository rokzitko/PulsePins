# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Dependency-free timeline builder for host-side PulsePins workflows."""

from dataclasses import dataclass
from fractions import Fraction
from html import escape
from typing import Dict, List, Optional, Tuple


class TimelineError(ValueError):
    """Raised when a timeline cannot be compiled safely."""


@dataclass(frozen=True)
class _Channel:
    name: str
    bit: int


@dataclass(frozen=True)
class _Pulse:
    channel: str
    start: int
    duration: int

    @property
    def end(self) -> int:
        return self.start + self.duration


class Timeline:
    """Build PulsePins text sequences from named-channel pulses.

    ``Timeline`` stores pulses internally in streamer clock cycles. Use
    ``unit="cycles"`` for raw cycle counts, or pass ``clock_hz`` with one of
    ``"ns"``, ``"us"``, ``"ms"``, or ``"s"`` to describe pulse times in an
    absolute time unit.
    """

    _UNIT_SECONDS = {
        "cycles": None,
        "cycle": None,
        "ns": Fraction(1, 1000000000),
        "us": Fraction(1, 1000000),
        "ms": Fraction(1, 1000),
        "s": Fraction(1, 1),
    }

    def __init__(
        self,
        unit: str = "cycles",
        clock_hz: Optional[float] = None,
        width: int = 32,
        initial_value: int = 0,
    ):
        if unit not in self._UNIT_SECONDS:
            raise TimelineError("unsupported timeline unit: {!r}".format(unit))
        if width <= 0:
            raise TimelineError("width must be positive")
        self.unit = unit
        self.clock_hz = clock_hz
        self.width = width
        self.initial_value = self._validate_value(initial_value, "initial_value")
        if self._UNIT_SECONDS[unit] is not None and clock_hz is None:
            raise TimelineError("clock_hz is required when unit is {!r}".format(unit))
        if clock_hz is not None and self._as_fraction(clock_hz, "clock_hz") <= 0:
            raise TimelineError("clock_hz must be positive")
        self._channels = {}  # type: Dict[str, _Channel]
        self._pulses = []  # type: List[_Pulse]

    def channel(self, name: str, bit: int):
        """Define a named output channel and return ``self`` for chaining."""
        if not name:
            raise TimelineError("channel name must not be empty")
        if name in self._channels:
            raise TimelineError("channel already exists: {!r}".format(name))
        if not isinstance(bit, int) or isinstance(bit, bool):
            raise TimelineError("channel bit must be an integer")
        if bit < 0 or bit >= self.width:
            raise TimelineError(
                "channel bit {} is outside the configured {}-bit output".format(
                    bit, self.width
                )
            )
        if any(channel.bit == bit for channel in self._channels.values()):
            raise TimelineError("channel bit {} is already in use".format(bit))
        self._channels[name] = _Channel(name, bit)
        return self

    def pulse(self, channel: str, start, duration):
        """Add one high pulse on ``channel`` and return ``self`` for chaining."""
        if channel not in self._channels:
            raise TimelineError("unknown channel: {!r}".format(channel))
        start_cycles = self._to_cycles(start, "start")
        duration_cycles = self._to_cycles(duration, "duration")
        if start_cycles < 0:
            raise TimelineError("start must be non-negative")
        if duration_cycles <= 0:
            raise TimelineError("duration must be positive")
        self._pulses.append(_Pulse(channel, start_cycles, duration_cycles))
        return self

    @property
    def channels(self) -> Tuple[Tuple[str, int], ...]:
        """Return defined channels as ``(name, bit)`` tuples."""
        return tuple((channel.name, channel.bit) for channel in self._channels.values())

    @property
    def pulses(self) -> Tuple[Tuple[str, int, int], ...]:
        """Return pulses as ``(channel, start_cycles, duration_cycles)`` tuples."""
        return tuple((pulse.channel, pulse.start, pulse.duration) for pulse in self._pulses)

    def duration_cycles(self) -> int:
        """Return the cycle count covered by all pulses."""
        return max((pulse.end for pulse in self._pulses), default=0)

    def records(self) -> List[Tuple[int, int]]:
        """Return compiled ``(duration_cycles, value)`` records."""
        self._validate_overlaps()
        if not self._pulses:
            return []

        channel_mask = self._channel_mask()
        points = {0}
        for pulse in self._pulses:
            points.add(pulse.start)
            points.add(pulse.end)
        ordered = sorted(points)

        records = []  # type: List[Tuple[int, int]]
        for start, end in zip(ordered, ordered[1:]):
            duration = end - start
            if duration <= 0:
                continue
            active_bits = 0
            for pulse in self._pulses:
                if pulse.start <= start and start < pulse.end:
                    active_bits |= 1 << self._channels[pulse.channel].bit
            value = (self.initial_value & ~channel_mask) | active_bits
            if records and records[-1][1] == value:
                records[-1] = (records[-1][0] + duration, value)
            else:
                records.append((duration, value))
        return records

    def to_sequence(
        self,
        force_trigger: bool = False,
        include_final: bool = False,
        final_value: Optional[int] = None,
    ) -> str:
        """Compile the timeline to PulsePins text sequence format."""
        lines = ["d {} 0x{:x}".format(duration, value) for duration, value in self.records()]
        if include_final:
            if final_value is None:
                final_value = self.initial_value & ~self._channel_mask()
            lines.append("final 0x{:x}".format(self._validate_value(final_value, "final_value")))
        if force_trigger:
            lines.append("f")
        return "\n".join(lines) + ("\n" if lines else "")

    def to_svg(self, width: int = 720, row_height: int = 28) -> str:
        """Return an SVG preview suitable for notebooks."""
        if width <= 0:
            raise TimelineError("SVG width must be positive")
        if row_height <= 0:
            raise TimelineError("SVG row_height must be positive")
        self._validate_overlaps()

        label_width = 112
        right_pad = 16
        top_pad = 22
        bottom_pad = 24
        plot_width = max(1, width - label_width - right_pad)
        rows = max(1, len(self._channels))
        height = top_pad + rows * row_height + bottom_pad
        total = max(1, self.duration_cycles())

        parts = [
            '<svg xmlns="http://www.w3.org/2000/svg" width="{}" height="{}" viewBox="0 0 {} {}">'.format(
                width, height, width, height
            ),
            '<rect width="100%" height="100%" fill="#ffffff"/>',
            '<line x1="{}" y1="{}" x2="{}" y2="{}" stroke="#999"/>'.format(
                label_width, top_pad - 8, label_width + plot_width, top_pad - 8
            ),
        ]

        for row, channel in enumerate(self._channels.values()):
            y = top_pad + row * row_height
            center_y = y + row_height / 2
            parts.append(
                '<text x="8" y="{}" font-family="monospace" font-size="12" dominant-baseline="middle">{}</text>'.format(
                    center_y, escape("{}[{}]".format(channel.name, channel.bit))
                )
            )
            parts.append(
                '<line x1="{}" y1="{}" x2="{}" y2="{}" stroke="#ddd"/>'.format(
                    label_width, center_y, label_width + plot_width, center_y
                )
            )

            for pulse in self._pulses:
                if pulse.channel != channel.name:
                    continue
                x = label_width + (pulse.start * plot_width / total)
                rect_width = max(1, pulse.duration * plot_width / total)
                parts.append(
                    '<rect x="{:.3f}" y="{:.3f}" width="{:.3f}" height="{}" rx="3" fill="#2563eb"/>'.format(
                        x, y + 4, rect_width, max(1, row_height - 8)
                    )
                )

        parts.append(
            '<text x="{}" y="{}" font-family="monospace" font-size="11" fill="#555">0 cycles</text>'.format(
                label_width, height - 6
            )
        )
        parts.append(
            '<text x="{}" y="{}" text-anchor="end" font-family="monospace" font-size="11" fill="#555">{} cycles</text>'.format(
                label_width + plot_width, height - 6, total
            )
        )
        parts.append("</svg>")
        return "".join(parts)

    def _repr_svg_(self) -> str:
        return self.to_svg()

    def _channel_mask(self) -> int:
        mask = 0
        for channel in self._channels.values():
            mask |= 1 << channel.bit
        return mask

    def _validate_overlaps(self) -> None:
        by_channel = {}  # type: Dict[str, List[Tuple[int, int]]]
        for pulse in self._pulses:
            by_channel.setdefault(pulse.channel, []).append((pulse.start, pulse.end))
        for channel, intervals in by_channel.items():
            previous_end = None
            for start, end in sorted(intervals):
                if previous_end is not None and start < previous_end:
                    raise TimelineError(
                        "overlapping pulses on channel {!r}".format(channel)
                    )
                previous_end = end

    def _validate_value(self, value: int, name: str) -> int:
        if not isinstance(value, int) or isinstance(value, bool):
            raise TimelineError("{} must be an integer".format(name))
        if value < 0 or value >= (1 << self.width):
            raise TimelineError(
                "{} must fit in the configured {}-bit output".format(name, self.width)
            )
        return value

    def _to_cycles(self, value, name: str) -> int:
        value_fraction = self._as_fraction(value, name)
        if self._UNIT_SECONDS[self.unit] is None:
            cycles = value_fraction
        else:
            assert self.clock_hz is not None
            cycles = value_fraction * self._UNIT_SECONDS[self.unit] * self._as_fraction(
                self.clock_hz, "clock_hz"
            )
        if cycles.denominator != 1:
            raise TimelineError(
                "{}={!r} {} is not an integer number of cycles".format(
                    name, value, self.unit
                )
            )
        return int(cycles)

    @staticmethod
    def _as_fraction(value, name: str) -> Fraction:
        if isinstance(value, bool):
            raise TimelineError("{} must be numeric".format(name))
        try:
            return Fraction(str(value))
        except (ValueError, ZeroDivisionError) as exc:
            raise TimelineError("{} must be numeric".format(name)) from exc
