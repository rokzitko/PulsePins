# Readback wrapper coverage

`test2` checks offline-compressed public output and an independent bit-serial
CRC over 256 input samples, with unrelated clocks and randomized downstream
stalls. It verifies pulse-count, CRC, mode, empty and overflow status registers,
then resets capture and checks zeroed counters/CRC before repeating.

Both stored mode-register values are exercised in the normal build, where
sampling remains valid/qin_clk. Actual optional strobe-clock operation is
checked separately by `../tb_rl_encoder/test6`.
