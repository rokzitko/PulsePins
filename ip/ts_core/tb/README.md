# Timestamp reference regression

`test2` predicts capture time from the external input schedule and clock epoch,
checking exact data and validity every cycle on both outputs. An eight-bit
timebase wraps naturally. Coverage includes simultaneous channels, independently
stalled sinks, a new capture
coincident with acceptance of the previous record, held pending records, reset
while pending, and overflow status/count readback.

For the 32-bit overflow saturation boundary only, the initial counter value is
seeded at `0xfffffffe` with a one-cycle force/release; real subsequent events
must saturate at `0xffffffff`
and public clear commands must restore zero. Timestamp expectations never read
the DUT timebase or edge-detector state.
