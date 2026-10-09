# Activity detection boundaries

`test2` checks both public activity outputs cycle by cycle for windows 0, 1 and
8. Asynchronous pulses and a coalesced burst fit entirely between clock edges;
the event latch must still detect them. The test verifies inclusive window
expiry, synchronous pulses, cancellation during synchronization, reset while
active and successful capture after reset. Reset is driven as a real edge so
the asynchronous latch is initialized before stimulus.
