# ppwebgui

`ppwebgui` is a standalone host-side web server for PulsePins.

It starts an embedded HTTP server, serves a small browser UI from the same binary, shows live AUX and trigger status, reports the current trigger-combiner configuration, lets the user change a single active-streamer qout override and the output-combiner settings, and can stream PulsePins text sequences from the browser.

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

When bound to `0.0.0.0`, `ppwebgui` also prints the currently discovered non-loopback IPv4 URLs so you can connect directly from another machine without looking up the address separately.

Example with an explicit port:

```bash
ppwebgui -port 8080
```

Example with an auto-selected free port:

```bash
ppwebgui -port 0
```

## Browser UI

The page exposes these main sections:

* Status Provenance: a legend explaining which values are live hardware polls, which are tracked by `ppwebgui`, and which form edits are still local to the browser
* Live Hardware: AUX bits, trigger bits, trigger enable/force/reset flags, and the live streamer runtime status word
* Tracked by ppwebgui: displayed qout, tracked idle streamer qout, output-override state, combiner mode, trigger mode, and recent action/error text
* Trigger Settings: tracked read-only trigger combiner mode plus invert and mask settings for the result, INT, EXT, MISC, and AUX paths
* Output Override: one manual final-output override control for the active streamer path used by browser-triggered sequence playback
* Output Combiner: mode selection plus per-output and per-input invert/mask/force settings
* Sequence: a text-area for PulsePins sequence text, a force-trigger checkbox, a readback-check checkbox, and a start button

Browser-triggered streams first run the same hardware reset/bring-up sequence exposed by the **Reset hardware** button, then append the currently tracked idle raw qout value as the final output element. That keeps each run deterministic and starts the streamer from a clean reset state.

The header also includes a **Reset hardware** button. That action reruns the same FPGA-side bring-up sequence used by `ppwebgui` startup, including the FPGA reset-manager pulse, startup clock/PLL policy, and the startup frequency-meter report. After that it reapplies the current web-managed combiner and output-override settings so the browser state is preserved across the reset.

The backend keeps hardware access serialized and the UI polls `/api/status` at the configured interval.

The current implementation restores live polling only for register paths that have been stable on the deployed hardware: AUX input state and the streamer runtime status word. Trigger-combiner settings, combiner routing, and the displayed qout values remain controller-managed snapshots so the web GUI does not re-enter the crashy register read paths.

While the user is editing the **Output Override** or **Output Combiner** form, the browser marks that form as a local edit until **Apply** is pressed. That makes it explicit when the visible form contents differ from the tracked state coming back from `/api/status`.

Values shown in the browser are rendered in hexadecimal by default. Input fields still accept the same integer formats as the CLI helpers: decimal, hexadecimal, binary, octal, and Verilog-style literals.

## API summary

Version 1 keeps the API small:

* `GET /api/status` returns JSON status for AUX, trigger state, trigger-combiner settings, active streamer qout state, combiner state, and recent action/error text
* `POST /api/qout` expects an `application/x-www-form-urlencoded` body with `override_enabled` and `override_value`
* `POST /api/combiner` expects an `application/x-www-form-urlencoded` body with the combiner mode plus output and input settings
* `POST /api/reset` reruns the `ppwebgui` FPGA bring-up path and reapplies the current web-managed settings
* `POST /api/stream` expects an `application/x-www-form-urlencoded` body with `sequence_text` and optional `force_trigger` and `check_readback`; before streaming, the backend reruns the `ppwebgui` hardware reset/bring-up path and then appends the current tracked idle raw qout as the final output value

The current implementation rejects oversized form submissions and limits `sequence_text` to 32 KiB per request.

Successful mutating `POST` replies include:

* `ok`
* `rc`
* `message`
* `status`

The embedded browser UI uses that returned `status` object immediately after a successful action, so the page does not have to wait for the next polling tick before reflecting the applied state.

The `status` payload also includes a `stream` object with:

* `last_rc`
* `message`
* `runtime.raw`
* `runtime.buffer_error`
* `runtime.done`
* `runtime.triggered`
* `runtime.armed`

and a `streamer` object with:

* `qout`
* `qout_streamer`
* `override.enabled`
* `override.value`

and a `trigger_settings` object with:

* `mode`
* `invert_result`
* `invert_int`
* `invert_ext`
* `invert_misc`
* `invert_aux`
* `mask_int`
* `mask_ext`
* `mask_misc`
* `mask_aux`

`POST /api/stream` runs synchronously. While a hardware action is in flight, the browser disables the control forms until the request finishes.

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
