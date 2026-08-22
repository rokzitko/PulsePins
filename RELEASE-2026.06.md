# PulsePins 2026.06

PulsePins 2026.06 is a major update since the first public release. It keeps the same goal - turning a DE10-Nano into an open-hardware digital pulse sequencer for lab timing and control - but substantially improves robustness, tooling, and documentation.

What's new since v1.0:

- New released DE10-Nano SD-card image using the current 32-bit output design and host/FPGA ABI 6.
- Many reliability fixes in the streamer, trigger path, clock-domain crossings, DMA playback, readback, reset sequencing, and timeout handling.
- Stronger validation paths: expanded self-tests, improved `run_all_tests_forever` burn-in logging, board smoke automation, host-side CI, and more RTL/IP testbenches.
- More useful user interfaces: improved command-line tools, SCPI server, browser UI, workstation Python SCPI client, and Python Timeline helpers for generated pulse programs.
- Expanded sequence workflows, including text, binary snapshot, VCD import/export, readback capture, replay, and clearer final-output behavior.
- Better clocking and measurement support, including PLL validation, frequency-meter improvements, timestamp/counter updates, and clearer clock-reporting behavior.
- Much broader documentation: quick start, tool chooser, worked examples, sequence format, Python workflow, web interface, build/deployment notes, and hardware references.
- Updated PP_PMOD shield documentation and design artifacts.

Validation note: this image has survived extensive burn-in testing over many weeks on two separate boards.

Asset:

- `pulsepins_2026_06.zip`
- SHA-256: `9454475c90db0025674209101aa3e9e18a8fec632db21f57af8978b669e1eebc`

Upgrade note: this release uses ABI 6. Keep the host tools and FPGA image from the same release together.
