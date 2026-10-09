# Preprocessor reference regression

`test5` runs on Questa or Icarus. A test-only command interpreter records stores
and expands finite replay requests independently, comparing every accepted
96-bit output under randomized stalls. It covers all eight storage positions,
pass/discard commands, randomized replay lengths/repetitions, and reset during
both finite and infinite replay. Infinite replay has a bounded checked prefix;
reset cancels the remaining stream, after which a fresh marker must be delivered.
