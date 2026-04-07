# ppwebgui

`ppwebgui` is a standalone host-side web server for PulsePins.

It starts an embedded HTTP server, serves a small browser UI from the same binary, shows live AUX and trigger status, lets the user change qout overrides and output-combiner settings, and can stream PulsePins text sequences from the browser.

## v1 scope

The first version is intentionally simple:

* HTTP only
* no authentication
* browser polling for live status updates
* embedded HTML, CSS, and JavaScript with no frontend build step

## Startup

`ppwebgui` follows the same shared host-side startup path as the other standalone host tools:

1. construct `HostRuntime`
2. apply the normal startup policy and create the shared `FPGA` wrapper
3. measure and report the active clocks
4. start the embedded HTTP server

Command-line options:

* `-ip <addr>`: bind address, default `0.0.0.0`
* `-port <n>`: bind port, default `4242`
* `-poll_ms <n>`: status sampling interval in milliseconds, default `100`

Example:

```bash
ppwebgui
```

This starts the server on all interfaces on port `4242` and prints the bound URL to standard output.

Example with an explicit port:

```bash
ppwebgui -port 8080
```

Example with an auto-selected free port:

```bash
ppwebgui -port 0
```

## Browser UI

The page exposes four main sections:

* Live Status: AUX bits, trigger bits, and trigger enable/force/reset flags
* Streamer Overrides: `q1` through `q4` override values with one apply action
* Output Combiner: mode selection plus per-output and per-input invert/mask/force settings
* Sequence: a text-area for PulsePins sequence text, a force-trigger checkbox, a readback-check checkbox, and a start button

The backend keeps hardware access serialized and the UI polls `/api/status` at the configured interval.

## Default network exposure

By default, `ppwebgui` binds to `0.0.0.0:4242`.

That means it accepts connections from outside the board, including from another machine over the Ethernet interface, as long as the network path is reachable.

If you want local-only access, bind explicitly to loopback instead:

```bash
ppwebgui -ip 127.0.0.1
```

## Security note

Version 1 does not provide authentication or HTTPS.

Treat the default `0.0.0.0` bind as network-exposed control access. On shared or untrusted networks, prefer a more restrictive bind address or external network controls.

## Related pages

* [ppscpi](ppscpi.md)
* [pptool](pptool.md)
* [build](build.md)
