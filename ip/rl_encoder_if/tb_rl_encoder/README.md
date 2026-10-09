# Encoder reference tests

`test5` checks the public encoded stream with unrelated input/read clocks and
randomized downstream stalls. A canonical RLE model predicts values/counts;
four-bit counts force natural run splitting at 15. Valid gaps flush final runs.
Pending-data reset, FIFO overflow, overflow reset and clean recovery are required.

`test6` builds with `WEIRD_CLOCK` and repeats these checks in both data-clock and
strobe-clock modes. Strobe-mode stimulus switches the clock selection only while
both candidate clocks are low. Normal builds continue to use valid/data-clock
sampling; the optional mode remains controlled by the existing build define.
