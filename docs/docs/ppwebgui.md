# ppwebgui

`ppwebgui` is a standalone web server that runs on the target board for rapid interactive testing and hardware troubleshooting.

It starts an embedded HTTP server on the target board, serves a small browser UI from the same binary, shows live AUX signal status, trigger-input/control, and streamer-runtime status, reports the current trigger-combiner configuration, lets the user change a single active-streamer qout override and the output-combiner settings, and can stream the canonical [PulsePins text sequence format](sequence_format.md) from the browser.

The code is intentionally simple (HTTP only, no authentication, browser polling for live status updates,
embedded HTML, CSS, and JavaScript).

## Startup

`ppwebgui` follows the same shared startup path as the other standalone target-board tools:

1. construct `HostRuntime`
2. apply the requested FPGA reset, clock/PLL, and LED startup settings and create the shared `FPGA` wrapper
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
It also prints a red `WARNING` line, states `waiting one second`, and waits one second before accepting browser traffic.

When bound to `0.0.0.0`, `ppwebgui` also prints the discovered non-loopback interface names, IPv4 addresses, and matching URLs so you can connect directly from another computer without looking up the address separately.

Example with an explicit port:

```bash
ppwebgui -port 8080
```

Example with an auto-selected free port:

```bash
ppwebgui -port 0
```

## Architecture

The `ppwebgui` implementation is split into small layers with a strict hardware-ownership boundary:

* [`ppwebgui_main.cc`]({{ source_file("c++/ppwebgui_main.cc") }}): contains `main()` and process startup
* [`ppwebgui_app.cc`]({{ source_file("c++/ppwebgui_app.cc") }}): composition root; anchors the single `WebGuiController` and starts the application
* [`ppwebgui_server.cc`]({{ source_file("c++/ppwebgui_server.cc") }}): owns the embedded `httplib::Server` lifecycle, bind/listen flow, and startup URL reporting
* [`ppwebgui_http.cc`]({{ source_file("c++/ppwebgui_http.cc") }}): request parsing, validation, error handling, and route registration
* [`ppwebgui_json.cc`]({{ source_file("c++/ppwebgui_json.cc") }}): JSON/status rendering
* [`ppwebgui_assets.cc`]({{ source_file("c++/ppwebgui_assets.cc") }}): embedded browser HTML/CSS/JS
* [`ppwebgui_config.cc`]({{ source_file("c++/ppwebgui_config.cc") }}): resolves CLI/input state into pure config values
* [`ppwebgui_service_api.cc`]({{ source_file("c++/ppwebgui_service_api.cc") }}): non-owning service interface bridge used by the GUI/HTTP side
* [`ppwebgui_service.cc`]({{ source_file("c++/ppwebgui_service.cc") }}): hardware-owning controller implementation

The runtime flow is:

1. [`ppwebgui_main.cc`]({{ source_file("c++/ppwebgui_main.cc") }}) creates `HostRuntime` and resolves `WebGuiRuntimeConfig`
2. [`ppwebgui_app.cc`]({{ source_file("c++/ppwebgui_app.cc") }}) anchors `WebGuiController` and creates a non-owning `WebGuiService` adapter
3. [`ppwebgui_server.cc`]({{ source_file("c++/ppwebgui_server.cc") }}) builds `httplib::Server` and starts the listener
4. [`ppwebgui_http.cc`]({{ source_file("c++/ppwebgui_http.cc") }}) converts requests into value requests for the service layer
5. [`ppwebgui_service.cc`]({{ source_file("c++/ppwebgui_service.cc") }}) executes those requests through `WebGuiController` and its hardware wrappers
6. [`ppwebgui_json.cc`]({{ source_file("c++/ppwebgui_json.cc") }}) renders returned value snapshots/results into responses


## Browser UI

The page exposes these main sections:

* Status Provenance: a legend explaining which values are live hardware polls, which are tracked by `ppwebgui`, and which form edits are still local to the browser (i.e. not applied yet)
* Live Hardware: AUX bits, trigger bits, trigger enable/force/reset flags, and the streamer status
* Tracked by ppwebgui: displayed qout, tracked idle streamer qout, output-override state, combiner mode, trigger mode, and recent action/error text
* Clocking: a read-only display of the current streamer clock source, tracked `int_clk`/`ext_clk` selection, `core_clk` and `int_clk` PLL profiles, and the last measured `ext_clk`, `int_clk`, `streamer_clk`, and `core_clk` frequencies
* Trigger Settings: trigger mode plus editable invert and mask settings for the result, INT, EXT, and MISC paths; AUX invert and mask are also visible (read-only). The default trigger mode is `INT`; EXT inversion is configured explicitly with `invert_ext`.
* Output Override: one manual final-output override control for the active streamer path used by browser-triggered sequence playback
* Output Combiner: mode selection plus per-output and per-input invert/mask/force settings
* Timeline Composer: web-browser editor for simple multi-channel pulse timelines in raw cycles or absolute time units, with JSON draft and pulse-table CSV import/export
* Sequence: a text-area for PulsePins sequence text, a force-trigger checkbox, a readback-check checkbox, and a start button

The clocking form exposes strict preset PLL profile strings from [`c++/pll_rules.hh`]({{ source_file("c++/pll_rules.hh") }}) through pulldown menus for both `core_clk` and `int_clk`, with `100M` as the initial default choice. If `ppwebgui` starts from a nonstandard PLL profile, the current value is shown in the menu so the browser state remains accurate. Raw `N,M,C` profiles and non-preset frequency strings are accepted only when they satisfy the same Cyclone V PLL legality checks as [`pllcalc`](pllcalc.md).

Applying clock settings reruns the same web-controller reset/bring-up path used by **Reset hardware**, then remeasures all four clocks and returns the updated snapshot. The displayed frequencies are therefore measurement snapshots, not live-polled readbacks. If startup used no explicit source request or a raw `-clk` selector, that current source is shown read-only until the user explicitly applies a managed `int_clk` or `ext_clk` choice.

The trigger form uses the same semantic mode names as the CLI trigger tool:

* `INT`
* `EXT`
* `MISC`
* `ANY`
* `ALL`

The mode selection and inversion fields are independent: for example, active-low external trigger lines can be handled with `ANY` plus `invert_ext = 0xffffffff`.

Browser-triggered streams first run the same web-controller reset/bring-up sequence exposed by the **Reset hardware** button. That resets the streamer core, readback encoder, and counters for a deterministic run. The tracked idle raw qout value is used as the final output element. Because `ppwebgui` selects the final output value, sequence text submitted through the browser must not contain an explicit `final ...` record, and shared random-final overrides such as `PP_RANDOM_FINAL` conflict with browser playback.

The **Timeline Composer** is a graphical interface layered on top of the existing sequence text path. It lets users define named output-bit channels and pulse intervals in raw cycles, `ns`, `us`, `ms`, or `s`. It validates that pulses on the same channel do not overlap after conversion to cycles. The result can be reviewed as an SVG timing diagram. The output `d <cycles> <value>` records are copied into the **Sequence** text area once the browser has a status snapshot. Cycle input is integer-only. Absolute-time input accepts decimals and rounds each start/duration to the nearest streamer-clock cycle; durations that round to zero cycles are rejected. For managed `int_clk`, conversion uses the nominal tracked `int_clk` PLL profile, including the standard strict presets, legal raw `N,M,C` triplets, and parseable frequency strings. For managed `ext_clk`, conversion uses the last measured `ext_clk` rounded to a lab-friendly decimal value within `0.1%`; unmanaged sources fall back to the last measured streamer clock. Clicking a preview lane adds a pulse to that channel at the clicked time using the **Preview click duration** field and the selected unit; dragging a preview lane creates a pulse spanning the dragged interval. Hovering a valid lane highlights the row, shows the insertion cursor, and draws a ghost pulse for the configured duration. Clicking an existing preview pulse or non-input area of a pulse-table row selects it and enables **Delete selected pulse**; when focus is not in a text field, `Delete` or `Backspace` removes the selected pulse and `Escape` clears the selection. **Export draft** writes a JSON draft containing the selected unit, channels, and pulses; **Import draft** restores that JSON. **Export CSV** writes one pulse row per interval with `channel,bit,start,duration,color`; **Import CSV** rebuilds channels from those rows using the selected pulse time unit and leaves the timeline unchanged if parsing fails. Unassigned output bits preserve the tracked qout state, timeline-owned bits are driven high only during their pulses, and no `final` record is generated by the browser because the backend appends the tracked qout final value.

Raw timeline clock profiles must be legal profiles accepted by the backend; unsafe raw PLL divider triplets are rejected instead of being used for nominal timing.

When **Check readback** is enabled, `ppwebgui` uses the same conservative default timeouts as the shared CLI workflow: 2 s waiting for the first readback element and 2 s for later idle gaps. Finite browser-triggered streams also inherit the internal 10 s streamer-completion timeout used by the shared playback path. A timeout during streaming is reported back to the browser as an HTTP `504` result, with the message text distinguishing readback wait timeout from streamer-completion timeout.

The header also includes a **Reset hardware** button. That action reapplies the tracked clock source and PLL settings, resets the streamer core, readback encoder, and counters, remeasures the clocks, and reapplies the current web-managed trigger, combiner, and output-override settings so the browser state is preserved across the reset. If the tracked clock source came from an unmanaged startup/raw selector state, that exact state is preserved instead of being coerced to `int_clk`.

The backend keeps hardware access serialized and the UI polls `/api/status` at the configured interval.


## Maintainer note

`ppwebgui` keeps a strict ownership boundary between the GUI/HTTP layer and the hardware-facing controller.

- `WebGuiController` is the anchored owner of the hardware object.
- Higher layers must not copy, move, or re-own that controller or the FPGA-facing wrappers beneath it.
- UI code should access the controller only through pointer/reference-based adapters that forward value requests and value snapshots.

On the GUI side, this split also keeps the remaining non-hardware dependencies explicit through small value objects:

- `WebGuiRuntimeConfig` for resolved startup/runtime settings
- `WebGuiAssets` for the embedded browser payload
- `WebGuiServerBinding` for bind/listen inputs
- `WebGuiHttpOptions` for route-side logging settings

Those types let the app, server, and HTTP layers depend on each other through pure values instead of reaching across modules for hidden globals or broader runtime objects.

The hardware-facing side should continue to expose only:

- value requests such as combiner changes, output override changes, reset requests, and stream launch requests
- value snapshots/results such as `StatusSnapshot`, `ResetResult`, and `StreamResult`

Higher layers should never own or relocate the hardware wrapper directly.

While the user is editing the **Trigger Settings**, **Output Override**, or **Output Combiner** form, the browser keeps those local edits visible until **Apply** or **Revert local edits** is pressed. That makes it explicit when the visible form contents differ from the tracked state coming back from `/api/status`.

The **Clocking** form behaves the same way. Local clock edits stay in the browser until **Apply clock settings** or **Revert local edits** is pressed, while the measured frequency fields continue to reflect the last captured snapshot. When the tracked source is unmanaged, the current source display remains read-only and the pulldown selects the next managed source to adopt if the user clicks **Apply clock settings**.

Values shown in the browser are rendered in hexadecimal by default. Input fields also accept the same integer formats as the CLI helpers: decimal, hexadecimal, binary, octal, and Verilog/SystemVerilog-style literals.

## API summary

All `POST` endpoints require an `application/x-www-form-urlencoded` request, including bodyless
actions such as measurement and reset. The browser UI sends an empty `URLSearchParams` body for
those actions.

* `GET /api/status` returns JSON status for AUX, trigger state, trigger-combiner settings, active streamer qout state, combiner state, and recent action/error text
* `POST /api/clocking` expects an `application/x-www-form-urlencoded` body with managed `source` (`int_clk` or `ext_clk`), `core_profile`, and `int_profile`; malformed requests or invalid preset/raw/frequency profile strings return HTTP `400`, while valid requests rerun the web-controller reset/bring-up path and then remeasure all clocks
* `POST /api/clocking/measure` reruns the frequency-meter measurement path without changing tracked clock settings
* `POST /api/trigger` expects an `application/x-www-form-urlencoded` body with `mode`, `invert_result`, `invert_int`, `invert_ext`, `invert_misc`, `mask_int`, `mask_ext`, and `mask_misc`
* `POST /api/qout` expects an `application/x-www-form-urlencoded` body with `override_enabled` and `override_value`
* `POST /api/combiner` expects an `application/x-www-form-urlencoded` body with the combiner mode plus output and input settings
* `POST /api/reset` reruns the `ppwebgui` controller reset/bring-up path and reapplies the current web-managed settings
* `POST /api/stream` expects an `application/x-www-form-urlencoded` body with non-empty canonical PulsePins `sequence_text` and optional `force_trigger` and `check_readback`; when supplied, `force_trigger` overrides the text's force request. Before streaming, the backend reruns the `ppwebgui` controller reset/bring-up path and then appends the current tracked idle raw qout as the final output value, so submitted text must not already contain `final ...` and `PP_RANDOM_FINAL` should not be set for browser playback. Explicit request-validation failures return HTTP `400`, although generic text-parser failures currently reach the HTTP `500` handler; hardware timeouts return HTTP `504`.

The backend rejects oversized form submissions and limits `sequence_text` to 32 KiB per request.

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

and a `clocking` object with:

* `tracked.source`
* `tracked.source_display`
* `tracked.source_managed`
* `tracked.core_profile`
* `tracked.int_profile`
* `measured.ext_clk_hz`
* `measured.int_clk_hz`
* `measured.streamer_clk_hz`
* `measured.core_clk_hz`

`POST /api/stream` runs synchronously. While a hardware action is in flight, the browser disables the control forms until the request finishes.

## Default network exposure

By default, `ppwebgui` binds to `0.0.0.0:4242`.

That means it accepts connections from outside the target board, including from another computer over the Ethernet interface, as long as the network path is reachable.
On this bind address, startup prints the external-interface warning and pauses for one second.

If you want local-only access, bind explicitly to loopback instead:

```bash
ppwebgui -ip 127.0.0.1
```

## Security note

`ppwebgui` does not provide authentication or HTTPS.

Treat the default `0.0.0.0` bind as network-exposed control access. On shared or untrusted networks, prefer a more restrictive bind address or external network controls.

## Related pages

* [PulsePins text sequence format](sequence_format.md)
* [ppscpi](ppscpi.md)
* [pptool](pptool.md)
* [build](build.md)
