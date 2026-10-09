# Random-generator boundaries

`test2` covers configured delay ranges 1–1, 1–2 and 3–10. It checks both endpoints
of the delay-selection function, bounds on actual delay reloads and event-phase
counters, and that output enable forces the output low while freezing all PRNG
and event-engine state. Re-enabled output must resume activity. These are
scheduling bounds, not a claim that every visible output transition is an event:
glitches and bursts contain their own segments.
