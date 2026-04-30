# ppwebgui

`ppwebgui` is a standalone host-side web server for PulsePins.

It starts an embedded HTTP server, serves a small browser UI from the same binary, shows live AUX, trigger-input/control, and streamer-runtime status, reports the current trigger-combiner configuration, lets the user change a single active-streamer qout override and the output-combiner settings, and can stream PulsePins text sequences from the browser.

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

When bound to `0.0.0.0`, `ppwebgui` also prints the currently discovered non-loopback interface names, IPv4 addresses, and matching URLs so you can connect directly from another machine without looking up the address separately.

Example with an explicit port:

```bash
ppwebgui -port 8080
```

Example with an auto-selected free port:

```bash
ppwebgui -port 0
```

## Architecture

The current `ppwebgui` implementation is split into small layers with a strict hardware-ownership boundary:

* `ppwebgui_main.cc`: process entry only
* `ppwebgui_app.cc`: composition root; anchors the single `WebGuiController`
* `ppwebgui_server.cc`: owns the embedded `httplib::Server` lifecycle, bind/listen flow, and startup URL reporting
* `ppwebgui_http.cc`: request parsing, validation, error handling, and route registration
* `ppwebgui_json.cc`: JSON/status rendering
* `ppwebgui_assets.cc`: embedded browser HTML/CSS/JS
* `ppwebgui_config.cc`: resolves CLI/input state into pure config values
* `ppwebgui_service_api.cc`: non-owning service interface bridge used by the GUI/HTTP side
* `ppwebgui_service.cc`: hardware-owning controller implementation

The runtime flow is:

1. `ppwebgui_main.cc` creates `HostRuntime` and resolves `WebGuiRuntimeConfig`
2. `ppwebgui_app.cc` anchors `WebGuiController` and creates a non-owning `WebGuiService` adapter
3. `ppwebgui_server.cc` builds `httplib::Server` and starts the listener
4. `ppwebgui_http.cc` converts requests into value requests for the service layer
5. `ppwebgui_service.cc` executes those requests against the hardware-owning wrapper graph
6. `ppwebgui_json.cc` renders returned value snapshots/results into responses

## Browser UI

The page exposes these main sections:

* Status Provenance: a legend explaining which values are live hardware polls, which are tracked by `ppwebgui`, and which form edits are still local to the browser
* Live Hardware: AUX bits, trigger bits, trigger enable/force/reset flags, and the live streamer runtime status word
* Tracked by ppwebgui: displayed qout, tracked idle streamer qout, output-override state, combiner mode, trigger mode, and recent action/error text
* Clocking: a read-only display of the current tracked streamer clock source, editable managed `int_clk`/`ext_clk` selection, tracked `core_clk` and `int_clk` PLL profiles, and the last measured `ext_clk`, `int_clk`, `streamer_clk`, and `core_clk` frequencies
* Trigger Settings: tracked trigger mode plus editable invert and mask settings for the result, INT, EXT, and MISC paths; AUX invert and mask remain visible read-only
* Output Override: one manual final-output override control for the active streamer path used by browser-triggered sequence playback
* Output Combiner: mode selection plus per-output and per-input invert/mask/force settings
* Timeline Composer: a dependency-free browser-side editor for simple multi-channel pulse timelines in raw cycles or absolute time units, with JSON draft and pulse-table CSV import/export
* Sequence: a text-area for PulsePins sequence text, a force-trigger checkbox, a readback-check checkbox, and a start button

The clocking form exposes preset PLL profile strings from `c++/pll_rules.hh` through pulldown menus for both `core_clk` and `int_clk`, with `100M` as the initial default choice. If `ppwebgui` starts from a nonstandard current PLL profile, the current value is still surfaced in the menu so the browser state remains accurate. Non-preset frequency strings are accepted and resolved with the same strict Cyclone V calculator as [`pllcalc`](pllcalc.md).

Applying clock settings reruns the same hardware reset/bring-up path used by **Reset hardware**, then remeasures all four clocks and returns the updated snapshot. The displayed frequencies are therefore measurement snapshots, not live-polled readbacks. If startup used no explicit source request or a raw `-clk` selector, that current source is shown read-only until the user explicitly applies a managed `int_clk` or `ext_clk` choice.

The trigger form uses the same semantic mode names as the CLI trigger tool:

* `STANDARD`
* `INT`
* `EXT`
* `MISC`
* `ANY`
* `ALL`

`STANDARD` matches the CLI meaning: it selects the OR combiner path and forces all EXT trigger lines inverted. In the browser UI, `EXT invert` becomes read-only while `STANDARD` is selected so the visible form state stays consistent with the applied semantics.

Browser-triggered streams first run the same hardware reset/bring-up sequence exposed by the **Reset hardware** button, then append the currently tracked idle raw qout value as the final output element. That keeps each run deterministic and starts the streamer from a clean reset state. Because `ppwebgui` owns that final-output policy, sequence text submitted through the browser must not also contain an explicit `final ...` record.

The **Timeline Composer** is a browser-only helper layered on top of the existing sequence text path. It lets users define named output-bit channels and pulse intervals in raw cycles, `ns`, `us`, `ms`, or `s`, validates that pulses on the same channel do not overlap after conversion to cycles, previews the result as an SVG timing diagram, and writes ordinary `d <cycles> <value>` records into the **Sequence** text area once the browser has a status snapshot. Cycle input stays integer-only. Absolute-time input accepts decimals and rounds each start/duration to the nearest streamer-clock cycle; durations that round to zero cycles are rejected. For managed `int_clk`, conversion uses the nominal tracked `int_clk` PLL profile, including the standard presets, raw `N,M,C` triplets, and parseable frequency strings. For managed `ext_clk`, conversion uses the last measured `ext_clk` rounded to a lab-friendly decimal value within `0.1%`; unmanaged sources fall back to the last measured streamer clock. Clicking a preview lane adds a pulse to that channel at the clicked time using the **Preview click duration** field and the currently selected unit; dragging a preview lane creates a pulse spanning the dragged interval. Hovering a valid lane highlights the row, shows the insertion cursor, and draws a ghost pulse for the configured duration. Clicking an existing preview pulse or non-input area of a pulse-table row selects it and enables **Delete selected pulse**; when focus is not in a text field, `Delete` or `Backspace` removes the selected pulse and `Escape` clears the selection. Normal validation still catches overlaps. **Export draft** writes a portable JSON draft containing the selected unit, channels, and pulses; **Import draft** restores that JSON without contacting the backend and leaves the current browser timeline unchanged if the draft is malformed. **Export CSV** writes one pulse row per interval with `channel,bit,start,duration,color`; **Import CSV** rebuilds channels from those rows using the currently selected pulse time unit and leaves the current timeline unchanged if parsing fails. Unassigned output bits preserve the current tracked qout state, timeline-owned bits are driven high only during their pulses, and no `final` record is generated by the browser because the backend appends the current tracked qout final value.

When **Check readback** is enabled, `ppwebgui` uses the same default safe timeout policy as the shared CLI/SCPI workflow: 2s waiting for the first readback element and 2s for later idle gaps. Finite browser-triggered streams also inherit the internal 10 s streamer-completion timeout used by the shared playback path. A timeout during streaming is reported back to the browser as an HTTP `504` result, with the message text distinguishing readback wait timeout from streamer-completion timeout.

The header also includes a **Reset hardware** button. That action reruns the same FPGA-side bring-up sequence used by `ppwebgui` startup, including the FPGA reset-manager pulse, tracked clock/PLL policy, and the startup frequency-meter report. After that it reapplies the current web-managed trigger, combiner, and output-override settings so the browser state is preserved across the reset. If the tracked source came from an unmanaged startup/raw selector state, that exact state is preserved instead of being coerced to `int_clk`.

The backend keeps hardware access serialized and the UI polls `/api/status` at the configured interval.

The current implementation restores live polling only for register paths that have been stable on the deployed hardware: AUX input state, external trigger input/control status, and the streamer runtime status word. Trigger-combiner settings, combiner routing, the clocking snapshot, and the displayed qout values remain controller-managed snapshots so the web GUI does not re-enter the crashy register read paths.

## Maintainer note

`ppwebgui` keeps a strict ownership boundary between the GUI/HTTP layer and the hardware-facing controller.

- `WebGuiController` is the anchored owner of the hardware object graph.
- Higher layers must not copy, move, or re-own that controller or the FPGA-facing wrappers beneath it.
- Route/UI code should access the controller only through pointer/reference-based adapters that forward value requests and value snapshots.

This rule is not only architectural hygiene. Refactors that changed the storage or ownership of the hardware controller, while leaving the logical operations the same, have caused board-only crashes in the output-override path. Preserve the anchored controller instance and add indirection around it instead of relocating it.

On the GUI side, the current split also keeps the remaining non-hardware dependencies explicit through small value objects:

- `WebGuiRuntimeConfig` for resolved startup/runtime policy
- `WebGuiAssets` for the embedded browser payload
- `WebGuiServerBinding` for bind/listen inputs
- `WebGuiHttpOptions` for route-side logging policy

Those types let the app, server, and HTTP layers depend on each other through pure values instead of reaching across modules for hidden globals or broader runtime objects.

The hardware-facing side should continue to expose only:

- value requests such as combiner changes, output override changes, reset requests, and stream launch requests
- value snapshots/results such as `StatusSnapshot`, `ResetResult`, and `StreamResult`

Higher layers should never own or relocate the hardware wrapper graph directly.

While the user is editing the **Trigger Settings**, **Output Override**, or **Output Combiner** form, the browser keeps those local edits visible until **Apply** or **Revert local edits** is pressed. That makes it explicit when the visible form contents differ from the tracked state coming back from `/api/status`.

The **Clocking** form behaves the same way. Local clock edits stay in the browser until **Apply clock settings** or **Revert local edits** is pressed, while the measured frequency fields continue to reflect the last captured snapshot. When the tracked source is unmanaged, the current source display remains read-only and the pulldown selects the next managed source to adopt if the user clicks **Apply clock settings**.

Values shown in the browser are rendered in hexadecimal by default. Input fields still accept the same integer formats as the CLI helpers: decimal, hexadecimal, binary, octal, and Verilog-style literals.

## API summary

Version 1 keeps the API small:

* `GET /api/status` returns JSON status for AUX, trigger state, trigger-combiner settings, active streamer qout state, combiner state, and recent action/error text
* `POST /api/clocking` expects an `application/x-www-form-urlencoded` body with managed `source` (`int_clk` or `ext_clk`), `core_profile`, and `int_profile`; malformed requests or invalid preset/raw/frequency profile strings return HTTP `400`, while valid requests rerun reset/bring-up and then remeasure all clocks
* `POST /api/clocking/measure` reruns the frequency-meter measurement path without changing tracked clock settings
* `POST /api/trigger` expects an `application/x-www-form-urlencoded` body with `mode`, `invert_result`, `invert_int`, `invert_ext`, `invert_misc`, `mask_int`, `mask_ext`, and `mask_misc`
* `POST /api/qout` expects an `application/x-www-form-urlencoded` body with `override_enabled` and `override_value`
* `POST /api/combiner` expects an `application/x-www-form-urlencoded` body with the combiner mode plus output and input settings
* `POST /api/reset` reruns the `ppwebgui` FPGA bring-up path and reapplies the current web-managed settings
* `POST /api/stream` expects an `application/x-www-form-urlencoded` body with `sequence_text` and optional `force_trigger` and `check_readback`; before streaming, the backend reruns the `ppwebgui` hardware reset/bring-up path and then appends the current tracked idle raw qout as the final output value, so submitted text must not already contain `final ...`. Request-validation failures return HTTP `400`; hardware timeouts return HTTP `504`.

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
