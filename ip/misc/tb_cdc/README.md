# CDC coherence and reset regression

`test2` drives encoded 64-bit payloads on an unrelated source clock. Every update
must be coherent, belong to a requested source value and carry the current reset
epoch. Continuous snapshots must also be coherent. The test requires blocked
acceptance, pending updates, convergence to a final known value, and cancellation
of in-flight requests across coupled logical resets of both domains.
