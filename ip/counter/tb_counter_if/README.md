# Counter wrapper tests

`test2` enables `DO_SUM2` and `COUNTER_CC`; `test3` builds the same reference test
with both disabled. `test4` enables only `DO_SUM2`, and `test5` only `COUNTER_CC`,
covering all four feature combinations. Unrelated clocks carry distinct channel sets
(including channels 0 and 31). Public readback is checked against input-history
level counts, crosscorrelation lag totals and squared run lengths. Disabled
features must return zero. Instrument reset is exercised during valid traffic,
with channel selections retained across that reset.
