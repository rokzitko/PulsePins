"""Losslessly encode runs using prefixes of PulsePins' eight replay slots."""


def compact_runs(runs, slots):
    if not 1 <= len(slots) <= 8:
        raise ValueError("Require 1..8 replay slots")
    # Minimize encoded records without changing any run's value or duration.
    costs = [0] * (len(runs) + 1)
    choices = [None] * len(runs)
    for start in range(len(runs) - 1, -1, -1):
        ticks, pins = runs[start]
        costs[start] = 1 + costs[start + 1]
        choices[start] = (start + 1, f"d {ticks} 0x{pins:x}")
        for length in range(1, len(slots) + 1):
            end = start
            while end + length <= len(runs) and runs[end:end + length] == slots[:length]:
                end += length
                if 1 + costs[end] < costs[start]:
                    costs[start] = 1 + costs[end]
                    choices[start] = (end, f"r {(end - start) // length} {length}")
    if costs[0] + len(slots) >= len(runs):
        return [f"d {ticks} 0x{pins:x}" for ticks, pins in runs]
    records = [f"store {slot} d {ticks} 0x{pins:x}" for slot, (ticks, pins) in enumerate(slots)]
    position = 0
    while position < len(runs):
        position, record = choices[position]
        records.append(record)
    return records
