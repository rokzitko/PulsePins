# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Dependency-free timeline builder for host-side PulsePins workflows."""

import csv
import io
import json
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
    color: str = ""


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

    def channel(self, name: str, bit: int, color: str = ""):
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
        if not isinstance(color, str):
            raise TimelineError("channel color must be a string")
        self._channels[name] = _Channel(name, bit, color)
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
    def channel_colors(self) -> Tuple[Tuple[str, str], ...]:
        """Return channel colors as ``(name, color)`` tuples."""
        return tuple((channel.name, channel.color) for channel in self._channels.values())

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

    def to_csv(self) -> str:
        """Export pulse rows in the browser Timeline Composer CSV format."""
        output = io.StringIO()
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(["channel", "bit", "start", "duration", "color"])
        for pulse in self._pulses:
            channel = self._channels[pulse.channel]
            writer.writerow(
                [
                    channel.name,
                    channel.bit,
                    self._cycles_to_unit_text(pulse.start),
                    self._cycles_to_unit_text(pulse.duration),
                    channel.color,
                ]
            )
        return output.getvalue()

    def to_draft(self) -> dict:
        """Return a browser Timeline Composer draft dictionary."""
        channel_index = {name: index for index, name in enumerate(self._channels)}
        return {
            "format": "pulsepins.timeline",
            "version": 1,
            "time_unit": self.unit,
            "channels": [
                {
                    "name": channel.name,
                    "bit": channel.bit,
                    "color": channel.color,
                }
                for channel in self._channels.values()
            ],
            "pulses": [
                {
                    "channel_index": channel_index[pulse.channel],
                    "start": self._cycles_to_unit_text(pulse.start),
                    "duration": self._cycles_to_unit_text(pulse.duration),
                }
                for pulse in self._pulses
            ],
        }

    def to_draft_json(self, indent: Optional[int] = 2) -> str:
        """Export a browser Timeline Composer draft JSON string."""
        return json.dumps(self.to_draft(), indent=indent) + "\n"

    @classmethod
    def from_draft_json(
        cls,
        text: str,
        clock_hz: Optional[float] = None,
        width: int = 32,
        initial_value: int = 0,
    ):
        """Import browser Timeline Composer draft JSON text."""
        try:
            draft = json.loads(text)
        except json.JSONDecodeError as exc:
            raise TimelineError("Draft JSON is invalid") from exc
        return cls.from_draft(
            draft, clock_hz=clock_hz, width=width, initial_value=initial_value
        )

    @classmethod
    def from_draft(
        cls,
        draft,
        clock_hz: Optional[float] = None,
        width: int = 32,
        initial_value: int = 0,
    ):
        """Import a browser Timeline Composer draft dictionary."""
        if not isinstance(draft, dict):
            raise TimelineError("Draft must be a JSON object")
        unit = str(draft.get("time_unit", draft.get("unit", "cycles")))
        channels = draft.get("channels")
        pulses = draft.get("pulses")
        if not isinstance(channels, list):
            raise TimelineError("Draft channels must be an array")
        if not isinstance(pulses, list):
            raise TimelineError("Draft pulses must be an array")

        timeline = cls(unit=unit, clock_hz=clock_hz, width=width, initial_value=initial_value)
        for index, channel in enumerate(channels):
            if not isinstance(channel, dict):
                raise TimelineError("Channel {} must be an object".format(index))
            name = str(channel.get("name", "CH{}".format(index + 1)))
            try:
                bit = int(str(channel.get("bit", index)), 0)
            except ValueError as exc:
                raise TimelineError("Channel {} has an invalid bit".format(index)) from exc
            color = str(channel.get("color", ""))
            timeline.channel(name, bit, color=color)

        for index, pulse in enumerate(pulses):
            if not isinstance(pulse, dict):
                raise TimelineError("Pulse {} must be an object".format(index))
            raw_channel_index = pulse.get("channel_index", pulse.get("channel"))
            channel_index = cls._draft_index(raw_channel_index, "Pulse {} channel_index".format(index))
            if channel_index >= len(timeline._channels):
                raise TimelineError(
                    "Pulse {} channel_index is outside the channel list".format(index)
                )
            channel_name = list(timeline._channels)[channel_index]
            start = str(pulse.get("start", "0"))
            duration = str(pulse.get("duration", "10"))
            timeline.pulse(channel_name, start=start, duration=duration)
        return timeline

    @classmethod
    def from_csv(
        cls,
        text: str,
        unit: str = "cycles",
        clock_hz: Optional[float] = None,
        width: int = 32,
        initial_value: int = 0,
    ):
        """Import browser Timeline Composer CSV text.

        Header rows are optional. Without a header, columns are interpreted as
        ``channel, bit, start, duration, color``.
        """
        rows = [row for row in csv.reader(io.StringIO(text)) if any(cell.strip() for cell in row)]
        if not rows:
            raise TimelineError("CSV has no pulse rows")
        header = cls._csv_header_map(rows[0])
        first_data_row = 1 if header is not None else 0
        columns = header or {"channel": 0, "bit": 1, "start": 2, "duration": 3, "color": 4}

        timeline = cls(unit=unit, clock_hz=clock_hz, width=width, initial_value=initial_value)
        channel_bits = {}  # type: Dict[str, int]
        for index, row in enumerate(rows[first_data_row:], start=first_data_row + 1):
            channel_name = cls._csv_cell(row, columns["channel"])
            bit_text = cls._csv_cell(row, columns["bit"])
            start_text = cls._csv_cell(row, columns["start"])
            duration_text = cls._csv_cell(row, columns["duration"])
            color = cls._csv_cell(row, columns["color"])
            if not channel_name:
                raise TimelineError("CSV row {} has an empty channel".format(index))
            if not bit_text:
                raise TimelineError("CSV row {} has an empty bit".format(index))
            if not start_text:
                raise TimelineError("CSV row {} has an empty start".format(index))
            if not duration_text:
                raise TimelineError("CSV row {} has an empty duration".format(index))
            try:
                bit = int(bit_text, 0)
            except ValueError as exc:
                raise TimelineError("CSV row {} has an invalid bit".format(index)) from exc
            if channel_name in channel_bits:
                if channel_bits[channel_name] != bit:
                    raise TimelineError(
                        "CSV row {} reuses channel {!r} with a different bit".format(
                            index, channel_name
                        )
                    )
            else:
                timeline.channel(channel_name, bit, color=color)
                channel_bits[channel_name] = bit
            timeline.pulse(channel_name, start=start_text, duration=duration_text)
        return timeline

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
                fill = escape(channel.color or "#2563eb", quote=True)
                parts.append(
                    '<rect x="{:.3f}" y="{:.3f}" width="{:.3f}" height="{}" rx="3" fill="{}"/>'.format(
                        x, y + 4, rect_width, max(1, row_height - 8), fill
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

    def to_vcd(self, timescale: str = "1ns", ticks_per_cycle: int = 1) -> str:
        """Export defined timeline channels as a simple scalar VCD trace.

        By default VCD timestamps use one tick per PulsePins cycle. For a
        100 MHz streamer clock, pass ``timescale="10ns"`` and keep
        ``ticks_per_cycle=1`` to make VCD time match wall-clock time.
        """
        if not isinstance(timescale, str) or not timescale.strip():
            raise TimelineError("timescale must be a non-empty string")
        if not isinstance(ticks_per_cycle, int) or isinstance(ticks_per_cycle, bool):
            raise TimelineError("ticks_per_cycle must be an integer")
        if ticks_per_cycle <= 0:
            raise TimelineError("ticks_per_cycle must be positive")
        self._validate_overlaps()

        channel_items = list(self._channels.values())
        ids = [self._vcd_identifier(index) for index in range(len(channel_items))]
        lines = [
            "$timescale {} $end".format(timescale.strip()),
            "$scope module pulsepins $end",
        ]
        for channel, identifier in zip(channel_items, ids):
            lines.append(
                "$var wire 1 {} {} $end".format(identifier, self._vcd_name(channel))
            )
        lines.extend(["$upscope $end", "$enddefinitions $end"])

        def emit_values(value: int) -> None:
            for channel, identifier in zip(channel_items, ids):
                bit = 1 if (value & (1 << channel.bit)) else 0
                lines.append("{}{}".format(bit, identifier))

        previous_value = None
        current_cycle = 0
        for duration, value in self.records():
            if value != previous_value:
                lines.append("#{}".format(current_cycle * ticks_per_cycle))
                emit_values(value)
                previous_value = value
            current_cycle += duration

        if previous_value is None:
            lines.append("#0")
            emit_values(self.initial_value & ~self._channel_mask())
        else:
            final_value = self.initial_value & ~self._channel_mask()
            if final_value != previous_value:
                lines.append("#{}".format(current_cycle * ticks_per_cycle))
                emit_values(final_value)
        return "\n".join(lines) + "\n"

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

    def _cycles_to_unit_text(self, cycles: int) -> str:
        if self._UNIT_SECONDS[self.unit] is None:
            return str(cycles)
        assert self.clock_hz is not None
        value = Fraction(cycles) / (
            self._UNIT_SECONDS[self.unit] * self._as_fraction(self.clock_hz, "clock_hz")
        )
        if value.denominator == 1:
            return str(value.numerator)
        return format(float(value), ".15g")

    @staticmethod
    def _csv_header_map(row) -> Optional[Dict[str, int]]:
        names = [cell.strip().lower() for cell in row]
        try:
            channel = names.index("channel")
            start = names.index("start")
            duration = names.index("duration")
        except ValueError:
            return None
        return {
            "channel": channel,
            "bit": names.index("bit") if "bit" in names else -1,
            "start": start,
            "duration": duration,
            "color": names.index("color") if "color" in names else -1,
        }

    @staticmethod
    def _csv_cell(row, index: int) -> str:
        return row[index].strip() if 0 <= index < len(row) else ""

    @staticmethod
    def _draft_index(value, label: str) -> int:
        text = str(value).strip()
        if not text.isdigit():
            raise TimelineError("{} must be a non-negative integer".format(label))
        return int(text)

    @staticmethod
    def _vcd_identifier(index: int) -> str:
        alphabet = [chr(code) for code in range(33, 127)]
        base = len(alphabet)
        index += 1
        chars = []
        while index:
            index, remainder = divmod(index - 1, base)
            chars.append(alphabet[remainder])
        return "".join(chars)

    @staticmethod
    def _vcd_name(channel: _Channel) -> str:
        name = "{}[{}]".format(channel.name, channel.bit)
        return "".join(ch if ch.isalnum() or ch in "_[]" else "_" for ch in name)

    @staticmethod
    def _as_fraction(value, name: str) -> Fraction:
        if isinstance(value, bool):
            raise TimelineError("{} must be numeric".format(name))
        try:
            return Fraction(str(value))
        except (ValueError, ZeroDivisionError) as exc:
            raise TimelineError("{} must be numeric".format(name)) from exc
