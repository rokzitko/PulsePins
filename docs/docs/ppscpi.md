## ppscpi

`ppscpi` is a standalone network server for remote control of a PulsePins device.

The implementation is in `c++/ppscpi.cc` and the SCPI session/server helpers are in `c++/scpi_server.hh`.

### Transport and startup

`ppscpi` listens on TCP port `5025`.

On startup it:

* configures realtime scheduling and locks memory pages
* resets the FPGA fabric
* creates the shared `HostRuntime` and top-level `FPGA` wrapper
* reports the measured clocks using the frequency-meter block
* accepts SCPI-style commands over the network

### Session model

Each client connection gets its own SCPI session object.

Session state includes:

* one `streamer`, `readback`, and `counter` wrapper set bound to the shared `FPGA`
* the currently loaded `Sequence`
* whether readback checking is enabled
* whether streaming should use forced triggering

The session does not persist across reconnects.

### Supported commands

Standard commands:

* `*IDN?` - identify the instrument
* `*RST` - clear loaded sequence and session state
* `*CLS` - clear status and error queue
* `*OPC` / `*OPC?` - operation complete flag/query
* `*WAI` - compatibility no-op; `STREAM` itself is synchronous
* `*ESR?` - standard event status register
* `*STB?` - status byte
* `SYST:ERR?` - query and drain the error queue

PulsePins-specific commands:

* `TEST1` - run a built-in short self-test sequence
* `SEQ <data>` - parse and load a sequence from textual representation, including `f`, `final`, and control-flow records supported by `parse_sequence_from_stream(...)`
* `CHECK <bool>` - enable or disable readback checking during `STREAM`
* `CHECK?` - query the current check setting
* `STREAM` - send the currently loaded sequence and trigger execution
* `DISCONNECT` - close the current client session; the `ppscpi` server keeps running
* `TERMINATE` - stop the `ppscpi` server process

### Typical flow

1. Connect to TCP port `5025`
2. Send `*RST`
3. Send `SEQ ...` with the sequence payload
4. Optionally send `CHECK ON`
5. Send `STREAM`
6. Query `SYST:ERR?` if needed

### Notes

* `STREAM` uses the same send/trigger path as the local tools, including optional readback verification.
* `SEQ` stores the parsed sequence in memory; nothing is transmitted to the streamer until `STREAM` is issued.
* Repeated `STREAM` commands reuse the stored sequence exactly as parsed; the cached session sequence is not rewritten by readback checking or final-output preparation.
* Before each hardware-touching run (`TEST1` and `STREAM`), `ppscpi` resets the streamer core, readback encoder, and counters so repeated commands in the same process/session start from a clean hardware state.
* If `TEST1` or `STREAM` returns `FAILURE`, `ppscpi` also pushes an execution-error record into `SYST:ERR?` with the aggregated PulsePins return-code bits so remote clients can distinguish timeout, readback, CRC, buffer, and overflow failures.
* When `CHECK ON` is active and no explicit startup `-timeout` was supplied, the shared workflow uses a conservative default readback timeout: 2s for the first readback element and 2s for later idle gaps. Start `ppscpi` with `-timeout 0` to disable that protection or `-timeout <value>` to override it.
* Hardware-touching commands are serialized across sessions through the shared FPGA lock, so multiple clients can stay connected without racing each other on streamer/reset state.
* The server is intended for remote orchestration, not for high-throughput binary bulk transfer.
* Command-handler exceptions are converted into SCPI error/status state instead of tearing down the whole server process.
* After `DISCONNECT`, clients can reconnect and start a fresh independent session.
* `TERMINATE` is the explicit server-shutdown command; it closes the current session and stops the process.

### Related pages

* `pptool.md`
* `cpp.md`
* `readback.md`
* `build.md`
