## ppscpi

`ppscpi` is a standalone network server for remote control of a PulsePins device.

The implementation is in `c++/ppscpi.cc` and the SCPI session/server helpers are in `c++/scpi_server.hh`.

### Transport and startup

`ppscpi` listens on TCP port `5025`.

On startup it:

* configures realtime scheduling and locks memory pages
* resets the FPGA fabric
* creates the normal `FPGA`, streamer, readback, and counter interfaces
* reports the measured clocks using the frequency-meter block
* accepts SCPI-style commands over the network

### Session model

Each client connection gets its own SCPI session object.

Session state includes:

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
* `*ESR?` - standard event status register
* `*STB?` - status byte
* `SYST:ERR?` - query and drain the error queue

PulsePins-specific commands:

* `TEST1` - run a built-in short self-test sequence
* `SEQ <data>` - parse and load a sequence from textual representation
* `CHECK <bool>` - enable or disable readback checking during `STREAM`
* `CHECK?` - query the current check setting
* `STREAM` - send the currently loaded sequence and trigger execution
* `TERMINATE` - terminate the session

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
* The server is intended for remote orchestration, not for high-throughput binary bulk transfer.

### Related pages

* `pptool.md`
* `cpp.md`
* `readback.md`
* `build.md`
