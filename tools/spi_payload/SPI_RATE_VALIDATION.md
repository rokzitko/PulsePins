# AD9959 SPI update-rate validation

## Current result: four-bit, 100 MHz SCLK

| Tested workload | Fastest fully passing soak | Evidence |
| --- | --- | --- |
| Bursts of 1-100 writes, 100-101 MHz | **120 ns/update; 8,333,333.333 updates/s** | 1,000/1,000 RF endpoints; 50,500 programmed writes; [report](results/spi-rate-20261111T154508-1q3wun9i/report.md) |
| Bursts of 9,991-10,000 writes, repeated shuffled ten-tone pattern | **720 ns/update; 1,388,888.889 updates/s** | 100/100 RF endpoints; 999,550 programmed writes; [report](results/spi-rate-20261111T154139-zqvkisu4/report.md) |

Both soaks passed all QOUT CRC and completion checks without output underruns.
The short-burst rate is **4.33 times** the earlier one-bit 100 MHz result.
Long bursts at 600 and 620 ns passed one trial but failed during repetition with
explicit output underruns; 720 ns is the fastest fully soaked long-burst setting
tested, not an exact sustained-rate boundary. Settings between 620 and 720 ns
were not soaked. No RF endpoint failure was observed after a valid playback.
Host measurement gaps separate bursts; individual RF transitions, electrical
margin and indefinite continuous operation remain unverified.

## Bench configuration

- PulsePins: `root@192.168.3.47`, installed ABI 5, streamer 100 MHz.
- AD9959: existing J3/J4 flywire harness, onboard 25 MHz reference, PLL x20.
- Flywire length: **20 cm**, reported by the operator on 2026-11-11.
- Current SCLK wiring: **J8 pin 3 (`STREAMER_CLK`/CLOCK) to DDS SCLK**, moved
  from Q0 and confirmed by the operator on 2026-11-11. The clock lead length
  has not been separately confirmed after this move.
- Four-bit wiring: **Q2..Q5 to SDIO_0..SDIO_3**, confirmed connected by the
  operator before the four-bit experiments on 2026-11-11. Q0 remains disconnected.
- CH1: full DAC current, amplitude code 1023; 100 frequencies, 100-101 MHz inclusive.
- Frequency increment: 1 MHz / 99 = 10,101.010101 Hz (FTWs rounded to 32 bits).
- SA: Siglent SVA1015X, `192.168.3.55`, CH1 connected to RF input.
- No sufficiently fast scope or logic analyzer is currently available. Proceed with
  SA endpoint measurements; connector edge quality and per-update RF timing remain unverified.
- Record ground arrangement, any probes and wiring changes in `--note` and the
  observations below. The ground arrangement has not yet been documented.

## Step 1: slow-update baseline, 10 MHz SCLK

Finish or stop the earlier one-hour sweep. Keep the existing CH1-to-SA connection.

```sh
python3 tools/spi_payload/spi_update_rate.py --mode slow --spi-clock 10e6 \
    --note "Original flywire harness; CH1 to SA"
```

The script holds each point for at least one second, extending this if measurement
takes longer. It uses a fixed 99.5-101.5 MHz display, RBW 1 kHz, VBW 3 kHz and
fresh peak measurements. The tone should advance in approximately 10.1 kHz steps.
The marker grid is approximately 2.67 kHz, so displayed steps need not be identical.

Acceptance: all 100 digital waveform CRC checks pass and all 100 held RF tones
are within 5 kHz of their nominal values and above -80 dBm. This tolerance allows
the module reference error and the trace grid while remaining below half a step.
Inspect the CSV for systematic frequency offset as well as missing/outlying points.
This validates writes at 10 MHz SCLK with generous gaps between updates.

## Step 2: inspect one dense burst

Deferred until suitable capture equipment is available. For the current SA-only
validation, proceed to Step 3 at the same SCLK before increasing it.

Connect a scope/logic analyzer at the **DDS end** of the flywires. Observe SCLK,
SDIO_0, CS and I/O_UPDATE with short probe ground connections. Record the probe
loading and sample rate. To validate every RF update, also capture CH1 RF with
adequate analog bandwidth and sample rate, or use a suitable IQ receiver.

```sh
python3 tools/spi_payload/spi_update_rate.py --mode burst --spi-clock 10e6 \
    --trials 20 --interval 1 --note "Add scope/probe details here"
```

Each trial sends one 100-point burst, then holds 101 MHz until the next trial.
The host-controlled inter-burst gap is not part of the maximum update-rate claim.
Trigger on the first UPDATE after the long gap; a holdoff longer than the burst
(e.g. 1 ms) helps avoid triggering on an interior update. Initialization also
contains UPDATE pulses; use a subsequent trial for the stress capture.

At 10 MHz expect:

- SCLK high/low: 50 ns during data; 40 rising edges per frequency write.
- UPDATE high/low minimum: 20 ns / 20 ns.
- UPDATE rising-edge spacing inside the burst: **4.12 us**.
- First-to-last UPDATE: **407.88 us** (99 intervals).
- Whole 100-write block: **412 us**, excluding two 10 ns surrounding idle records.
- Within-burst rate: **242,718.447 updates/s**.

Look for ringing, double clock crossings, degraded logic levels and data setup/hold
at the receiving connector. Decode the instruction `0x04` and the tuning words
against the saved sequence. The RF trace should show the intended staircase,
allowing DDS pipeline latency and reference error. Preserve raw scope/IQ captures
and screenshots alongside the run artifacts. A final tone or averaged SA spectrum
alone cannot prove that all intermediate writes took effect.

## Step 3: automated dense-burst endpoint sampling

```sh
python3 tools/spi_payload/spi_update_rate.py --mode endpoints --spi-clock 10e6
```

This sends prefixes of length 1, 2, ..., 100 and measures the final tone of each.
There is no corrective write after the burst. Acceptance: 100/100 digital CRCs
and 100/100 held endpoints pass. This samples the final write under increasingly
long dense traffic; it does not replace the time-resolved RF check in Step 2.

## Step 4: increase SCLK one setting at a time

Repeat Steps 1 and 3 at **12.5, 16.666667, 25, then 50 MHz**, recording actual SCLK.
Add Step 2 when capture equipment becomes available. SA-only results establish
the tested endpoint behavior, not correctness of every intermediate update.
The 100 MHz streamer cannot produce every requested clock with equal integer
high/low durations (e.g. a requested 20 MHz becomes 16.666667 MHz).

| Actual SCLK | Update interval | Within-burst rate |
| --- | --- | --- |
| 10 MHz | 4.12 us | 242,718.447/s |
| 12.5 MHz | 3.32 us | 301,204.819/s |
| 16.666667 MHz | 2.52 us | 396,825.397/s |
| 25 MHz | 1.72 us | 581,395.349/s |
| 50 MHz | 0.92 us | 1,086,956.522/s |

These are scheduled rates, not yet verified operating limits. On a failure, save
the evidence and repeat the last passing setting with the same wiring/probes.
Distinguish DMA underrun/CRC failure, flywire signal-integrity failure, and RF
measurement uncertainty. A later soak test can use more `--trials`; finite bursts
with host gaps do not establish an indefinitely sustained update rate.

## Step 5: forwarded 100 MHz SCLK experiment

Status: operator confirmed rewiring; **100/100 slow points and 100/100 dense-burst
endpoints passed**, followed by a **1,000/1,000 endpoint soak pass** on 2026-11-11.
The 10-50 MHz results below used Q0-generated SCLK.

- Move the DDS SCLK lead from Q0 to **J8 pin 3 (`STREAMER_CLK`/CLOCK)**. Disconnect
  Q0 from that lead; do not join the two outputs. Retain the existing CS, SDIO_0,
  I/O_UPDATE, reset and ground connections. J8 pin 5 is GND.
- The existing RTL directly exports `streamer_clk` on `GPI0GPIO2`
  (`pulsepins.sv`), so this uses the installed 100 MHz clock without a bitstream
  change. It runs continuously, including between sequences.
- AD9959 Rev. C, page 6 and Table 25 on page 31: maximum SCLK **200 MHz**,
  minimum high/low **2.2/1.6 ns**, data setup/hold **2.2/0 ns**, CS setup **1 ns**.
  Thus 100 MHz with nominal 5 ns high/low is within the chip's serial timing
  specification if receiver setup/hold and signal quality are satisfied.
- QOUT changes after rising streamer edges. The receiver model samples the prior
  QOUT state before the new data/CS arrives: first bit on the edge after assertion,
  last bit on the edge that launches deselection. For a data arrival delay `d`
  relative to the forwarded clock, this model needs `0 <= d <= 7.8 ns` before
  adding margin. CS has its own skew/setup constraint. Actual wiring skew remains
  unmeasured; `pulsepins.sdc` has no receiver-specific output timing contract.
- `--clock-source streamer` emits one bit per streamer tick and holds CS low
  for exactly the required number of clocks (40 for a five-byte FTW write).
  CS high ignores idle clock edges but only pauses the serial parser; it does
  not reset a misframed transaction (datasheet page 32).
- Each point schedules 20 ns deselected guard, 400 ns data, 60 ns deselected
  guard, then 20 ns UPDATE high and 20 ns low: **520 ns/update**, or
  **1,923,076.923 updates/s**. A 100-write burst occupies **52.02 us** including
  surrounding idle. These are generated timings, not measured RF update rates.
- CRC checks cover programmed QOUT data/control. The separately forwarded SCLK
  and its phase relative to QOUT are not included in those CRC checks.
- Offline tests decode all 100 writes against an independent continuous clock,
  check framing, initialization and UPDATE timing, and exercise assumed positive
  data delays of 0.5, 3.0 and 7.5 ns. They do not establish physical timing margin.
- Preparation on 2026-11-11: all 20 tests in `check-spi-update-rate` passed.
  The setup and all 100 Q0-clocked sequences still match the passing 50 MHz soak
  artifacts byte-for-byte. Forwarded-clock artifacts: [generated-only report](results/spi-rate-20261111T124919-uygs4l3n/report.md).

After the wiring change, run the slow baseline, then the dense endpoint test:

```sh
python3 tools/spi_payload/spi_update_rate.py --clock-source streamer --mode slow \
    --note "SCLK from J8 pin 3; record actual clock lead and ground arrangement"
python3 tools/spi_payload/spi_update_rate.py --clock-source streamer --mode endpoints \
    --note "SCLK from J8 pin 3; record actual clock lead and ground arrangement"
```

An explicit `--spi-clock 100e6` is optional in this mode; other values are rejected.
Use `--generate-only` to prepare artifacts without contacting hardware.

## Step 6: four-bit writes and update-spacing scan

Use `--clock-source streamer --serial-bits 4` with the confirmed wiring above.
AD9959 Rev. C Figure 45 (page 34) sends both instruction and data in four-bit
symbols: high nibble first, SDIO_3 carrying bit 7/3 and SDIO_0 carrying bit 4/0.
The eight-bit instruction plus 32-bit FTW therefore consumes **ten SCLK cycles**.

Initialization retains the known-good one-bit register writes and guards, then
writes **CSR=0x26** (CH1, four-bit, MSB-first) in one-bit mode and pulses UPDATE.
It waits another 1 us before quad traffic. SDIO_1..3 remain low until the switch.
The mode bits require UPDATE (Table 28 footnote); channel-enable bits act immediately.
Every new run resets and initializes the device, including after an underrun.

At 100 MHz SCLK, spacing in this non-overlapped waveform format is:

`(10 + pre_guard_ticks + post_guard_ticks + 2 * update_ticks) * 10 ns`

- Default 2/6/2 settings: **220 ns/update**, **4,545,454.545 updates/s**.
- Zero guards, 20 ns UPDATE high/low: **140 ns/update**, **7,142,857.143/s**.
- Zero guards, 10 ns UPDATE high/low: **120 ns/update**, **8,333,333.333/s**.
- At zero post guard, CS high and UPDATE high launch together after the last
  data symbol. CS remains high during UPDATE high and low. There are still
  exactly ten selected clocks per write under the forwarded-clock arrival model.
- The DDS SYNC_CLK period is 8 ns at 500 MHz system clock. Nominal 10 ns UPDATE
  exceeds the minimum pulse width, but is asynchronous to SYNC_CLK. Its physical
  width, setup margin and RF timing uncertainty have not been measured.

Run the slow baseline, then prefixes of lengths 1-100. Reduce guards in 10 ns
steps and retain all successes and failures. Shuffle the frequency list with
`--shuffle-seed` to exercise nonmonotonic FTWs. Use `--burst-cycles` for longer
uninterrupted waveforms: with ten points and 100 cycles, endpoint lengths are
991-1,000, not 1-10. Fixed analyzer range and 5 kHz tolerance remain unchanged.
Source snapshots, sequence hashes, full QOUT CRCs and playback diagnostics are
retained for every run. These checks exclude the forwarded SCLK itself.

The serialization-only ceiling at this SCLK is **10 million writes/s**. The
120 ns setting is the minimum of the implemented non-overlapped format, not an
established AD9959 limit; overlapping traffic and faster SCLK have not been tested.
All **24** tests in `make -C tools/spi_payload check-spi-update-rate` passed.
The one-bit setup and all 100 endpoint sequences still match both the 50 MHz
and forwarded 100 MHz passing soak artifacts byte-for-byte.

## Results ledger

| Date/run | SCLK | Test | Digital result | RF/physical result | Evidence |
| --- | --- | --- | --- | --- | --- |
| 2026-11-11 / `spi-rate-20261111T100247-tbgrxh1c` | 10 MHz | Slow baseline | Setup + 100/100 point CRC checks PASS; no underrun or raw-capture overflow | 100/100 RF points PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T100247-tbgrxh1c/report.md), [CSV](results/spi-rate-20261111T100247-tbgrxh1c/measurements.csv) |
| Deferred | 10 MHz | Time-resolved dense burst | Pending | Suitable scope/IQ capture equipment unavailable | |
| 2026-11-11 / `spi-rate-20261111T101019-iixrq07h` | 10 MHz | Endpoint sampling, uncompressed DMA | 69 bursts PASS; playback completion check failed on burst 70; run stopped | 69/69 measured endpoints PASS; burst 70 not measured | [Failed report](results/spi-rate-20261111T101019-iixrq07h/report.md), [Failure log](results/spi-rate-20261111T101019-iixrq07h/trial-000-point-0069.log) |
| 2026-11-11 / `spi-rate-20261111T101620-29cgjdhj` | 10 MHz | Endpoint sampling, compact replay | Setup + 100/100 burst CRC checks PASS; no output underrun; raw-capture overflow on bursts 11-100 | 100/100 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T101620-29cgjdhj/report.md), [CSV](results/spi-rate-20261111T101620-29cgjdhj/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T103107-ng479nfv` | 12.5 MHz | Slow baseline, compact replay | Setup + 100/100 point CRC checks PASS; no underrun or raw-capture overflow | 100/100 RF points PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T103107-ng479nfv/report.md), [CSV](results/spi-rate-20261111T103107-ng479nfv/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T103431-6cq_mwfp` | 12.5 MHz | Endpoint sampling, compact replay | Setup + 100/100 burst CRC checks PASS; no output underrun; raw-capture overflow on bursts 11-100 | 100/100 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T103431-6cq_mwfp/report.md), [CSV](results/spi-rate-20261111T103431-6cq_mwfp/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T110226-wxtg_u36` | 16.666667 MHz | Slow baseline, compact replay | Setup + 100/100 point CRC checks PASS; no underrun or raw-capture overflow | 100/100 RF points PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T110226-wxtg_u36/report.md), [CSV](results/spi-rate-20261111T110226-wxtg_u36/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T110532-5x3_6g0a` | 16.666667 MHz | Endpoint sampling, compact replay | Setup + 100/100 burst CRC checks PASS; no output underrun; raw-capture overflow on bursts 11-100 | 100/100 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T110532-5x3_6g0a/report.md), [CSV](results/spi-rate-20261111T110532-5x3_6g0a/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T111330-ivbkbbph` | 25 MHz | Slow baseline, compact replay | Setup + 100/100 point CRC checks PASS; no underrun or raw-capture overflow | 100/100 RF points PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T111330-ivbkbbph/report.md), [CSV](results/spi-rate-20261111T111330-ivbkbbph/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T111651-nr49erwp` | 25 MHz | Endpoint sampling, compact replay | Setup + 100/100 burst CRC checks PASS; no output underrun; raw-capture overflow on bursts 11-100 | 100/100 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T111651-nr49erwp/report.md), [CSV](results/spi-rate-20261111T111651-nr49erwp/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T113221-afq7o8fj` | 50 MHz | Slow baseline, compact replay | Setup + 100/100 point CRC checks PASS; no underrun or raw-capture overflow | 100/100 RF points PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T113221-afq7o8fj/report.md), [CSV](results/spi-rate-20261111T113221-afq7o8fj/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T113542-qftn_33z` | 50 MHz | Endpoint sampling, compact replay | Setup + 100/100 burst CRC checks PASS; no output underrun; raw-capture overflow on bursts 10-100 | 100/100 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T113542-qftn_33z/report.md), [CSV](results/spi-rate-20261111T113542-qftn_33z/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T121955-j0lrjxi8` | 50 MHz | 10-trial endpoint soak, compact replay | Setup + 1,000/1,000 burst CRC checks PASS; no output underrun; raw-capture overflow on 910 bursts | 1,000/1,000 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T121955-j0lrjxi8/report.md), [CSV](results/spi-rate-20261111T121955-j0lrjxi8/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T125235-z0pubm7q` | 100 MHz, forwarded | Slow baseline | Setup + 100/100 QOUT CRC checks PASS; no underrun or raw-capture overflow; forwarded SCLK outside CRC coverage | 100/100 RF points PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T125235-z0pubm7q/report.md), [CSV](results/spi-rate-20261111T125235-z0pubm7q/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T125556-if1flblp` | 100 MHz, forwarded | Endpoint sampling | Setup + 100/100 QOUT CRC checks PASS; no output underrun; raw-capture overflow on bursts 40-100; forwarded SCLK outside CRC coverage | 100/100 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T125556-if1flblp/report.md), [CSV](results/spi-rate-20261111T125556-if1flblp/measurements.csv) |
| 2026-11-11 / `spi-rate-20261111T134555-xh3pr71y` | 100 MHz, forwarded | 10-trial endpoint soak | Setup + 1,000/1,000 QOUT CRC checks PASS; no output underrun; raw-capture overflow on 610 bursts; forwarded SCLK outside CRC coverage | 1,000/1,000 endpoints PASS, max absolute error 2.950 kHz | [Report](results/spi-rate-20261111T134555-xh3pr71y/report.md), [CSV](results/spi-rate-20261111T134555-xh3pr71y/measurements.csv) |

No maximum reliable SPI clock or continuous end-to-end update rate has yet been established.
The one-bit results above pass through **100 MHz forwarded SCLK**, with
**0.52 us** between updates inside finite bursts (**1,923,076.923 updates/s**).
The later four-bit tests below reach **120 ns/update** in short bursts; longer
bursts exposed a playback underrun limit before any RF reception failure.
All five Q0-generated clock settings and the forwarded 100 MHz setting passed
the slow and dense-endpoint checks.
The 50 MHz endpoint soak also passed all **1,000 measurements** across ten
trials, covering **50,500 programmed updates** in finite bursts of lengths 1-100.
The forwarded 100 MHz endpoint soak likewise passed **1,000/1,000 measurements**
across ten trials and **50,500 programmed updates**, at 520 ns within-burst spacing.
50 MHz is the ceiling of Q0-generated SCLK at the current 100 MHz streamer
clock (one 10 ns tick per SCLK half-period). No RF endpoint reception failure
has been observed; the flywire/DDS limit has not been located.

### Baseline observations: 2026-11-11

- Requested and achieved SCLK: 10 MHz. Each point used one five-byte FTW write
  and one 20 ns UPDATE, followed by a host-paced hold for the SA measurement.
- Analyzer identity: `Siglent Technologies,SVA1015X,SVA1XEAC4R0005,3.2.2.6.2R5`.
- Fixed analyzer range: 99.5-101.5 MHz; RBW 1 kHz, VBW 3 kHz.
- Measured-minus-requested frequency: -2949.53 to -282.79 Hz; mean -1600.00 Hz.
  The systematic offset is consistent with the DDS reference error observed in
  earlier sweeps, with individual readings quantized by the SA trace grid.
- Measured peak levels: -8.19 to -8.02 dBm.
- Setup and all point playbacks matched both hardware CRCs to the independently
  computed waveform CRC. Raw-capture overflow was false throughout.
- Board logs identify bitstream timestamp `Jul 3 2026 07:41:53`, ABI 5, measured
  streamer and core clocks 100 MHz. Remote artifacts: `/home/root/spi-rate.llsvuG`.
- Final state: CH1 at nominal 101 MHz/full amplitude; SA continuous, fixed
  99.5-101.5 MHz. Dense-burst RF behavior and connector waveforms remain unmeasured.

### Dense-burst endpoint observations: 2026-11-11

- Harness: 20 cm flywires; CH1 to the SA; no fast scope/logic analyzer available.
- Initial uncompressed run passed burst lengths 1-69, then failed the streamer's
  completion/underrun check at length 70. That log did not distinguish timeout
  from underrun. The script stopped before taking an RF reading for that burst.
  This is a digital playback failure, not evidence of a flywire reception failure.
- Added lossless eight-slot replay encoding and deeper output-FIFO priming.
  The full burst now needs 2,373 encoded records instead of 8,451 expanded runs
  (plus the final record). Offline expansion exactly matches the original pin
  values and durations; waveform CRC and SCLK/UPDATE timing are unchanged.
  Failure diagnostics now log the actual underrun flag before resetting hardware.
- The retry passed all 100 burst lengths, totaling **5,050 programmed updates**
  and **100 measured endpoints**. Only each burst's endpoint was measured in RF.
- The 100-update burst contained 41,202 valid 100 MHz samples (412.02 us including
  surrounding idle), plus one final control sample. Streamer and readback CRCs
  both matched the expected `0x5b32c28b`; all FIFO accounting checks passed.
- Raw-capture FIFO overflow occurred for lengths 11-100. The full-stream readback
  CRC is calculated before that FIFO and matched on every burst; no lossless
  run-by-run capture is claimed. The setup and lengths 1-10 had no overflow.
- Endpoint frequency errors: -2949.53 to -282.79 Hz, mean -1600.00 Hz, matching
  the slow baseline within the analyzer's trace grid. Peak levels: -8.19 to -8.02 dBm.
- Remote artifacts: `/home/root/spi-rate.eBAUQK`. CH1 remains at nominal 101 MHz,
  full amplitude; the analyzer remains continuous at fixed 99.5-101.5 MHz.

### 12.5 MHz validation: 2026-11-11

- Requested and achieved SCLK: **12.5 MHz**, 40 ns high/low during SPI data.
  Hardware-timed UPDATE spacing: **3.32 us**, or **301,204.819 updates/s** within
  bursts. The 100-write block is 332 us, plus 20 ns surrounding idle.
- Conditions: existing 20 cm flywires, full amplitude, 100-101 MHz/100 points,
  compact replay and deeper FIFO priming; no fast scope/logic analyzer.
- Slow baseline: setup and 100/100 point waveform CRCs passed; 100/100 RF points
  passed. No raw-capture overflow or output underrun.
- Endpoint test: setup and 100/100 burst waveform CRCs passed; 100/100 RF endpoints
  passed, covering **5,050 programmed updates** in bursts of lengths 1-100.
  No output underrun. Raw-capture overflow occurred on lengths 11-100, while
  the full-stream readback CRC remained correct on every burst.
- The full burst had 33,202 valid 100 MHz samples and one final control sample.
  Expected, streamer and readback CRCs all matched `0x5552dbc4`; FIFO counts matched.
- Both runs: measured-minus-requested frequency -2949.53 to -282.79 Hz, mean
  -1600.00 Hz; peak levels -8.20 to -8.03 dBm. All 100 slow and dense endpoint
  frequency readings agreed at analyzer resolution and matched the 10 MHz baseline.
- Remote artifacts: slow `/home/root/spi-rate.awHmys`; endpoints
  `/home/root/spi-rate.M0jvtg`. Exact sequences, source snapshots, CSV and logs
  are retained in the linked run directories above.
- Final state checked: CH1 nominal 101 MHz/full amplitude; SA continuous at
  fixed 99.5-101.5 MHz, RBW 1 kHz, VBW 3 kHz.
- Per-update RF timing and receiving-end edge quality remain unmeasured.

### 16.666667 MHz validation: 2026-11-11

- Requested and achieved SCLK: **16.666667 MHz**, scheduled 30 ns high/low
  during SPI data. Hardware-timed UPDATE spacing: **2.52 us**, or
  **396,825.397 updates/s** within bursts. The 100-write block is 252 us,
  plus 20 ns surrounding idle.
- Conditions: existing 20 cm flywires, full amplitude, 100-101 MHz/100 points,
  compact replay and deeper FIFO priming; no fast scope/logic analyzer.
- Slow baseline: setup and 100/100 point waveform CRCs passed; 100/100 RF points
  passed. No raw-capture overflow or output underrun.
- Endpoint test: setup and 100/100 burst waveform CRCs passed; 100/100 RF endpoints
  passed, covering **5,050 programmed updates** in bursts of lengths 1-100.
  No output underrun. Raw-capture overflow occurred on lengths 11-100, while
  the full-stream readback CRC remained correct on every burst.
- The full burst had 25,202 valid 100 MHz samples and one final control sample.
  Expected, streamer and readback CRCs all matched `0x4975afcd`; FIFO counts matched.
- Both runs: measured-minus-requested frequency -2949.53 to -282.79 Hz, mean
  -1600.00 Hz. Peak levels: slow -8.21 to -8.04 dBm; endpoints -8.22 to -8.04 dBm.
  All 100 slow and dense endpoint frequency readings agreed at analyzer resolution.
- Remote artifacts: slow `/home/root/spi-rate.PB2YYM`; endpoints
  `/home/root/spi-rate.PCQO5B`. Exact sequences, source snapshots, CSV and logs
  are retained in the linked run directories above.
- Final state: CH1 nominal 101 MHz/full amplitude; analyzer settings checked:
  continuous, fixed 99.5-101.5 MHz, RBW 1 kHz, VBW 3 kHz.
- Per-update RF timing and receiving-end edge quality remain unmeasured.

### 25 MHz validation: 2026-11-11

- Requested and achieved SCLK: **25 MHz**, scheduled 20 ns high/low during SPI
  data. Hardware-timed UPDATE spacing: **1.72 us**, or **581,395.349 updates/s**
  within bursts. The 100-write block is 172 us, plus 20 ns surrounding idle.
- Conditions: existing 20 cm flywires, full amplitude, 100-101 MHz/100 points,
  compact replay and deeper FIFO priming; no fast scope/logic analyzer.
- Slow baseline: setup and 100/100 point waveform CRCs passed; 100/100 RF points
  passed. No raw-capture overflow or output underrun.
- Endpoint test: setup and 100/100 burst waveform CRCs passed; 100/100 RF endpoints
  passed, covering **5,050 programmed updates** in bursts of lengths 1-100.
  No output underrun. Raw-capture overflow occurred on lengths 11-100, while
  the full-stream readback CRC remained correct on every burst.
- The full burst had 17,202 valid 100 MHz samples and one final control sample.
  Expected, streamer and readback CRCs all matched `0xf675a2f3`; FIFO counts matched.
  The full burst used 2,354 encoded records plus the final record.
- Both runs: measured-minus-requested frequency -2949.53 to -282.79 Hz, mean
  -1600.00 Hz. Peak levels: slow -8.21 to -8.04 dBm; endpoints -8.22 to -8.04 dBm.
  All 100 slow and dense endpoint frequency readings agreed at analyzer resolution.
- Remote artifacts: slow `/home/root/spi-rate.LV4oek`; endpoints
  `/home/root/spi-rate.nYK77T`. Exact sequences, source snapshots, CSV and logs
  are retained in the linked run directories above.
- Final state: CH1 nominal 101 MHz/full amplitude; analyzer settings checked:
  continuous, fixed 99.5-101.5 MHz, RBW 1 kHz, VBW 3 kHz.
- Per-update RF timing and receiving-end edge quality remain unmeasured.

### 50 MHz validation: 2026-11-11

- Requested and achieved SCLK: **50 MHz**, scheduled 10 ns high/low during SPI
  data. Hardware-timed UPDATE spacing: **0.92 us**, or **1,086,956.522 updates/s**
  within bursts. The 100-write block is 92 us, plus 20 ns surrounding idle.
- Conditions: existing 20 cm flywires, full amplitude, 100-101 MHz/100 points,
  compact replay and deeper FIFO priming; no fast scope/logic analyzer.
- Slow baseline: setup and 100/100 point waveform CRCs passed; 100/100 RF points
  passed. No raw-capture overflow or output underrun.
- Endpoint test: setup and 100/100 burst waveform CRCs passed; 100/100 RF endpoints
  passed, covering **5,050 programmed updates** in bursts of lengths 1-100.
  No output underrun. Raw-capture overflow occurred on lengths **10-100**, while
  the full-stream readback CRC remained correct on every burst.
- The full burst had 9,202 valid 100 MHz samples and one final control sample.
  Expected, streamer and readback CRCs all matched `0x827a98ac`; FIFO counts matched.
  The full burst used 2,373 encoded records plus the final record.
- Both runs: measured-minus-requested frequency -2949.53 to -282.79 Hz, mean
  -1600.00 Hz; peak levels -8.22 to -8.05 dBm. All 100 slow and dense endpoint
  frequency readings agreed at analyzer resolution.
- Remote artifacts: slow `/home/root/spi-rate.YUGE7r`; endpoints
  `/home/root/spi-rate.4i1zVT`. Exact sequences, source snapshots, CSV and logs
  are retained in the linked run directories above.
- Final state: CH1 nominal 101 MHz/full amplitude; analyzer settings checked:
  continuous, fixed 99.5-101.5 MHz, RBW 1 kHz, VBW 3 kHz.
- Per-update RF timing, receiving-end edge quality and indefinitely sustained
  operation remain unmeasured. This is the highest tested passing SCLK and the
  Q0-generated clock ceiling, not an established maximum reliable DDS update rate.

### 50 MHz endpoint soak: 2026-11-11

- Run started at 12:19:55 UTC; **10 trials** of burst lengths 1-100 with the same
  20 cm flywires, full amplitude and 100-101 MHz frequency list. The DDS was
  initialized once before the trials. Minimum host interval: one second.
- **1,000/1,000 RF endpoints passed**, covering **50,500 programmed updates**.
  All ten trials returned the same frequency readings as the preceding 50 MHz
  endpoint test at analyzer resolution.
- Setup and **1,000/1,000 burst waveform CRC checks passed**, with no output
  underrun. Post-run checks verified all 101 sequence hashes and independently
  expanded waveform CRCs, then compared all 1,001 playback logs against them.
- Raw-capture overflow occurred on burst lengths 10-100 in every trial:
  **910 bursts** total. Setup and lengths 1-9 had no overflow. Both full-waveform
  hardware CRCs matched on every playback, including overflowing captures.
- Timing stayed at **0.92 us/update**, or **1,086,956.522 updates/s** within
  bursts. Each full 100-update burst occupied 92.02 us including surrounding
  idle; expected, output and readback CRCs all matched `0x827a98ac`.
- Frequency errors: -2949.53 to -282.79 Hz, mean -1600.00 Hz; peak levels:
  **-8.25 to -8.06 dBm**. Only burst endpoints were measured in RF.
- Remote artifacts: `/home/root/spi-rate.sDzka8`; exact sequences, source
  snapshots, build/playback logs, CSV and reports are retained in the linked
  local run directory above.
- Final state: CH1 nominal 101 MHz/full amplitude; analyzer settings checked:
  continuous, fixed 99.5-101.5 MHz, RBW 1 kHz, VBW 3 kHz.
- This confirms repeatable endpoint behavior for the tested finite bursts.
  Host measurement gaps separate bursts; continuous operation and individual
  RF transitions remain unverified.
- Reproduce the soak with:

  ```sh
  python3 tools/spi_payload/spi_update_rate.py --mode endpoints --spi-clock 50e6 \
      --trials 10 --note "50 MHz endpoint soak; existing 20 cm flywires; SA-only"
  ```

### 100 MHz forwarded-clock validation: 2026-11-11

- Operator confirmed moving DDS SCLK from Q0 to **J8 pin 3 CLOCK/STREAMER_CLK**.
  The original harness was reported as 20 cm; clock lead length after the move
  and ground arrangement were not separately measured. CH1 remains connected
  to the SVA1015X; no fast scope/logic analyzer was available.
- SCLK is the free-running streamer clock; the board frequency meter reported
  **100.000 MHz**. One data bit is scheduled per 10 ns, with 40 selected clock
  cycles per FTW write. UPDATE spacing is **520 ns** (**1,923,076.923 updates/s**)
  inside bursts; the complete 100-write sequence occupies **52.02 us** including
  surrounding idle. This is about 1.77 times the preceding 50 MHz burst rate.
- Slow baseline: setup and 100/100 QOUT waveform CRC checks passed; **100/100 RF
  points passed**. No raw-capture overflow or output underrun.
- Endpoint test: setup and 100/100 QOUT waveform CRC checks passed; **100/100 RF
  endpoints passed**, covering **5,050 programmed updates** in bursts of lengths
  1-100. No output underrun. Raw-capture overflow occurred on lengths **40-100**
  (61 bursts); setup and lengths 1-39 had no overflow.
- The full burst used 2,157 data records plus one final record, producing 5,202
  valid samples plus one final control sample. Expected, output and readback
  QOUT CRCs all matched `0x1a454079`; FIFO counts matched. Sequence hashes,
  independently expanded CRCs and all setup/playback logs were checked after
  both runs. The forwarded SCLK itself is outside this CRC coverage.
- Both runs: measured-minus-requested frequency -2949.53 to -282.79 Hz, mean
  -1600.00 Hz; RF peak levels **-8.24 to -8.07 dBm**. All 100 frequency readings
  agreed between slow, dense endpoints and the prior 50 MHz baseline at analyzer
  resolution.
- Remote artifacts: slow `/home/root/spi-rate.QBwKfJ`; endpoints
  `/home/root/spi-rate.Rfn1QW`. Exact sequences, source snapshots, CSV and logs
  are retained in the linked local run directories above.
- Final state: CH1 nominal 101 MHz/full amplitude; analyzer settings checked:
  continuous, fixed 99.5-101.5 MHz, RBW 1 kHz, VBW 3 kHz. SCLK remains connected
  to the free-running J8 clock output.
- Reception succeeded for these tested writes and endpoints. Receiving-end
  clock/data/CS skew, electrical timing margin, individual RF transitions and
  indefinitely sustained operation remain unmeasured.

### 100 MHz forwarded-clock endpoint soak: 2026-11-11

- Run started at **13:45:55 UTC**: ten trials of burst lengths 1-100, with DDS
  SCLK on J8 pin 3, full amplitude and the same 100-101 MHz frequency list.
  The DDS was initialized once before the trials. Minimum host interval: one second.
- **1,000/1,000 RF endpoints passed**, covering **50,500 programmed updates**.
  Setup and **1,000/1,000 QOUT waveform CRC checks passed** with no output underrun.
- All 101 sequence files matched the preceding 100 MHz endpoint run byte-for-byte.
  Post-run checks verified their SHA-256 hashes, independently expanded QOUT CRCs,
  and all 1,001 setup/playback logs. Q0 remained low throughout the sequences.
- Raw-capture overflow occurred on lengths **40-100** in every trial:
  **610 bursts** total. Setup and lengths 1-39 had no overflow. Both full-waveform
  QOUT CRCs matched on every playback; the forwarded clock and its phase remain
  outside this CRC coverage.
- Scheduled UPDATE spacing: **520 ns**, or **1,923,076.923 updates/s** within
  finite bursts. Each full burst occupied **52.02 us** including surrounding idle.
  Its expected, output and readback QOUT CRCs all matched `0x1a454079`.
- Frequency errors: **-2949.53 to -282.79 Hz**, mean **-1648.00 Hz**; peak levels:
  **-8.29 to -8.09 dBm**. Maximum absolute error stayed at 2.950 kHz, within the
  unchanged 5 kHz acceptance tolerance.
- Relative to the first 100 MHz endpoint run, 18 of 1,000 readings moved down one
  approximately 2.667 kHz SA trace bin: point 17 in all ten trials, and point 50
  in eight trials (all except trials 3 and 4). The remaining 982 readings matched.
  These bin changes did not cross the RF failure threshold; their physical cause
  was not determined by the endpoint measurements.
- Remote artifacts: `/home/root/spi-rate.iVsYU3`; exact sequences, source
  snapshots, build/playback logs, CSV and reports are retained in the linked
  local run directory above.
- Final state: CH1 nominal 101 MHz/full amplitude; analyzer settings checked:
  continuous, fixed 99.5-101.5 MHz, RBW 1 kHz, VBW 3 kHz. SCLK remains on J8 pin 3.
- The soak confirms repeatable endpoint reception in the tested finite-burst
  regime. Host measurement gaps separate bursts; individual RF transitions,
  physical clock/data timing margin and continuous operation remain unverified.
- Reproduce the soak with:

  ```sh
  python3 tools/spi_payload/spi_update_rate.py --clock-source streamer --mode endpoints \
      --trials 10 --note "100 MHz endpoint soak; SCLK from J8 pin 3; SA-only"
  ```

### Four-bit short-burst spacing scan: 2026-11-11

All tests used **100 MHz forwarded SCLK**, full amplitude, 100 frequencies over
100-101 MHz, and the fixed 99.5-101.5 MHz analyzer range. Each endpoint run sent
prefixes of lengths 1-100, totaling 5,050 programmed writes. Every completed
playback passed both QOUT CRCs and FIFO accounting, with no output underrun.
The baseline used one isolated write per point. The table's overflow counts
refer only to the raw-capture FIFO, not output playback or full-waveform CRCs.

| Test / spacing | Pre/post/UPDATE ticks | RF endpoints | Raw overflows | Evidence |
| --- | --- | --- | --- | --- |
| Slow baseline, 220 ns waveform | 2/6/2 | 100/100 | 0 | [Report](results/spi-rate-20261111T143337-nkmucp5w/report.md) |
| 220 ns | 2/6/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T143732-nl8rouzm/report.md) |
| 210 ns | 1/6/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T144045-7bpqdslf/report.md) |
| 200 ns | 0/6/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T144342-y8brfc9s/report.md) |
| 190 ns | 0/5/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T144657-leysz9pn/report.md) |
| 180 ns | 0/4/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T144952-mk65n2yz/report.md) |
| 170 ns | 0/3/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T145247-xe5m1fgz/report.md) |
| 160 ns | 0/2/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T145543-7lql986p/report.md) |
| 150 ns | 0/1/2 | 100/100 | 26 | [Report](results/spi-rate-20261111T145838-0m9qt0x3/report.md) |
| 140 ns | 0/0/2 | 100/100 | 19 | [Report](results/spi-rate-20261111T150152-xceq9f6h/report.md) |
| 130 ns | 0/1/1 | 100/100 | 29 | [Report](results/spi-rate-20261111T150556-gt2avw1g/report.md) |
| 120 ns | 0/0/1 | 100/100 | 21 | [Report](results/spi-rate-20261111T150852-7j_v2ery/report.md) |
| 120 ns, shuffled seed 42 | 0/0/1 | 100/100 | 22 | [Report](results/spi-rate-20261111T151427-lte7wi9b/report.md) |

The monotonic baseline and all eleven spacing settings had frequency errors
between -2949.53 and -363.60 Hz. The shuffled recheck had errors between
-2868.72 and -201.99 Hz. All were within the unchanged 5 kHz acceptance tolerance.
No corrective writes were sent between a burst and its RF measurement.
The full 120 ns monotonic burst occupies **12.02 us**, including surrounding
idle, with 1,052 data records plus the final record. Its expected, output and
readback CRCs are **`0x17807cc4`**. Continuously supplying this same record
density would require approximately **1.05 GB/s** of 12-byte DMA records;
the short-burst pass benefits from preloading into FPGA buffers.

### Four-bit longer-burst playback limits: 2026-11-11

The following runs used ten frequencies over 100-101 MHz, shuffled with seed 42,
then repeated in one uninterrupted waveform. They retain 100 MHz SCLK and
10 ns UPDATE high/low; extra spacing is a deselected post-write guard.
Failures below explicitly report **`done=0 buffer_error=1`**. A failed burst
has no RF measurement and is not counted as a DDS reception failure. Completed
earlier endpoints in the same failed run remain valid evidence.

| Spacing | Burst lengths | Result | Evidence |
| --- | --- | --- | --- |
| 120 ns | 991-1,000 | Output underrun on length 991; no RF measurement | [Report](results/spi-rate-20261111T151213-lt42ilr9/report.md), [Failure](results/spi-rate-20261111T151213-lt42ilr9/trial-000-point-0000.log) |
| 220 ns | 991-1,000 | Output underrun on length 991; no RF measurement | [Report](results/spi-rate-20261111T151724-b8453w_f/report.md), [Failure](results/spi-rate-20261111T151724-b8453w_f/trial-000-point-0000.log) |
| 320 ns | 991-1,000 | Output underrun on length 991; no RF measurement | [Report](results/spi-rate-20261111T151907-omb23gdm/report.md), [Failure](results/spi-rate-20261111T151907-omb23gdm/trial-000-point-0000.log) |
| 520 ns | 991-1,000 | Two digital/RF endpoints passed; underrun on length 993 | [Report](results/spi-rate-20261111T152022-78732w67/report.md), [Failure](results/spi-rate-20261111T152022-78732w67/trial-000-point-0002.log) |
| 920 ns | 991-1,000 | 10/10 digital/RF endpoints passed; no output underrun | [Report](results/spi-rate-20261111T152140-hq6zp2qs/report.md) |
| 920 ns | 9,991-10,000 | 10/10 digital/RF endpoints passed; no output underrun | [Report](results/spi-rate-20261111T152715-hqpyf34i/report.md) |
| 720 ns | 9,991-10,000 | 10/10 digital/RF endpoints passed | [Report](results/spi-rate-20261111T152956-w337p7bf/report.md) |
| 620 ns | 9,991-10,000 | 10/10 digital/RF endpoints passed; later soak failed | [Report](results/spi-rate-20261111T153146-d0obw9pj/report.md) |
| 570 ns | 9,991-10,000 | Four digital/RF endpoints passed; underrun on length 9,995 | [Report](results/spi-rate-20261111T153316-523zpzht/report.md), [Failure](results/spi-rate-20261111T153316-523zpzht/trial-000-point-0004.log) |
| 590 ns | 9,991-10,000 | Three digital/RF endpoints passed; underrun on length 9,994 | [Report](results/spi-rate-20261111T153439-0cpxu1cc/report.md), [Failure](results/spi-rate-20261111T153439-0cpxu1cc/trial-000-point-0003.log) |
| 600 ns | 9,991-10,000 | 10/10 digital/RF endpoints passed; later soak failed | [Report](results/spi-rate-20261111T153600-liycxno0/report.md) |
| 600 ns, ten-trial soak attempted | 9,991-10,000 | 12 digital/RF endpoints passed; underrun at trial 2, length 9,993 | [Report](results/spi-rate-20261111T153820-ipbryjsu/report.md), [Failure](results/spi-rate-20261111T153820-ipbryjsu/trial-001-point-0002.log) |
| 620 ns, ten-trial soak attempted | 9,991-10,000 | 19 digital/RF endpoints passed; underrun at trial 2, length 10,000 | [Report](results/spi-rate-20261111T153955-yd1gm3w3/report.md), [Failure](results/spi-rate-20261111T153955-yd1gm3w3/trial-001-point-0009.log) |
| 720 ns, ten-trial soak | 9,991-10,000 | 100/100 digital/RF endpoints passed; no output underrun | [Report](results/spi-rate-20261111T154139-zqvkisu4/report.md) |

The 920 ns run programmed 9,955 updates at **1,086,956.522/s** inside bursts.
All ten raw captures overflowed while both full QOUT CRCs passed. Frequency
errors were -2222.26 to -444.41 Hz. Short bursts can fit in FPGA buffering;
their rate must not be presented as an indefinitely sustainable DMA-fed rate.
The installed headers specify a 4,096-sample output FIFO and 12-byte DMA records.
Pin-transition density, run-length compression and buffer priming therefore
matter in addition to serial-clock speed. The failing subsystem is the output
playback path; these measurements do not isolate its internal bandwidth bottleneck.
The longer 920 ns run programmed another **99,955 updates**. Its full 10,000-write
waveform occupied **9.20002 ms** and contained 110,002 data records, plus final;
the average DMA-record demand was approximately **143.5 MB/s**. All ten RF
endpoints and full QOUT CRCs passed, with raw-capture overflow on each waveform.

### Four-bit repeatability results: 2026-11-11

**Short bursts, 120 ns/update:**

- [Ten-trial report](results/spi-rate-20261111T154508-1q3wun9i/report.md),
  [CSV](results/spi-rate-20261111T154508-1q3wun9i/measurements.csv),
  [evidence audit](results/spi-rate-20261111T154508-1q3wun9i/evidence-audit.json).
- Run started at 15:45:08 UTC. **1,000/1,000 RF endpoints passed**, covering
  **50,500 programmed writes** in monotonic prefixes of lengths 1-100.
- Zero pre/post guards; UPDATE high and low each 10 ns. The full 100-write
  waveform lasts **12.02 us**, including idle. This is **8,333,333.333 updates/s**
  inside the burst. All 101 sequences are identical to the first passing
  monotonic 120 ns run, with full-burst CRC **`0x17807cc4`**.
- Setup and all 1,000 waveform checks passed: both hardware CRCs, completion,
  final pins and FIFO counts. No output underrun. Raw-capture overflow occurred
  on lengths 80-100 in every trial, **210 bursts** total.
- RF errors: **-2949.53 to -282.79 Hz**, mean **-1648.00 Hz**; levels
  **-8.36 to -8.14 dBm**. Acceptance tolerance remained 5 kHz.
- Remote artifacts: `/home/root/spi-rate.YV3y60`.

**Long bursts, 720 ns/update:**

- [Ten-trial report](results/spi-rate-20261111T154139-zqvkisu4/report.md),
  [CSV](results/spi-rate-20261111T154139-zqvkisu4/measurements.csv),
  [evidence audit](results/spi-rate-20261111T154139-zqvkisu4/evidence-audit.json).
- Run started at 15:41:39 UTC. **100/100 RF endpoints passed**, covering
  **999,550 programmed writes** in bursts of lengths 9,991-10,000.
- Ten equally spaced frequencies over 100-101 MHz, shuffled with seed 42 and
  repeated without gaps inside each waveform. Zero pre guard, 600 ns post
  guard and 10 ns UPDATE high/low. Rate: **1,388,888.889 updates/s**.
- Each full burst lasts **7.20002 ms**, including idle; 110,002 data records
  plus final, CRC **`0x49fdab50`**. Average record demand is approximately
  **183.3 MB/s**. All eleven sequences match the first 720 ns run byte-for-byte.
- Setup and all 100 waveform checks passed, with no output underrun. All 100
  raw captures overflowed; both full QOUT CRCs still matched on every playback.
- RF errors: **-2222.26 to -444.41 Hz**, mean **-1333.33 Hz**; levels
  **-8.33 to -8.15 dBm**. Acceptance tolerance remained 5 kHz.
- Remote artifacts: `/home/root/spi-rate.xUyiy0`.

The failed 600/620 ns soaks show why a single passing trial is insufficient to
assign an operating rate. The 720 ns result applies to the tested waveform,
buffering and workload; neither indefinite operation nor all possible FTW
patterns have been validated. More compact playback or a dedicated FPGA serial
engine could reduce the transport bottleneck, but those changes were not tested.
The 120 ns result remains a finite, prebuffered short-burst result.

Post-run validation checked all 28 four-bit runs: every saved sequence's SHA-256,
expanded sample count and CRC, independent nibble decoding and UPDATE spacing,
plus all successful playback logs. Failed playback logs retain their underrun
flags. Per-run `evidence-audit.json` files summarize the checks. Analyzer state
was verified after the final short-burst soak: continuous 99.5-101.5 MHz, RBW
1 kHz, VBW 3 kHz; CH1 holds nominal **101 MHz at full amplitude**, with the DDS
still in four-bit mode and SCLK continuously running on J8 pin 3.

Reproduce the passing soaks:

```sh
python3 tools/spi_payload/spi_update_rate.py --clock-source streamer --serial-bits 4 \
    --mode endpoints --pre-guard-ticks 0 --post-guard-ticks 0 --update-ticks 1 --trials 10
python3 tools/spi_payload/spi_update_rate.py --clock-source streamer --serial-bits 4 \
    --mode endpoints --pre-guard-ticks 0 --post-guard-ticks 60 --update-ticks 1 \
    --points 10 --burst-cycles 1000 --shuffle-seed 42 --trials 10
```

### Operator observations

- Flywire length: 20 cm (operator report); ground arrangement: pending.
- SCLK rewired from Q0 to J8 pin 3 on 2026-11-11; separate clock lead length: pending.
- SDIO_1..3 connections confirmed before the four-bit experiments; Q2..Q5 map to SDIO_0..3.
- Suitable fast scope/logic analyzer: unavailable at present; using the SA for validation.
- Ringing/edge quality at DDS connector: unmeasured.
- Time-resolved RF staircase: unmeasured.
