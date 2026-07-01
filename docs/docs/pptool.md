# pptool

``pptool`` is a single executable for performing a range of tasks. Several symbolic links point to the same
executable. The exact functionality depends on the executable called.

The network-facing services and helper calculators are provided by separate standalone
executables, not by ``pptool`` symlinks. Those are ``ppscpi``, ``ppwebgui``, and ``pllcalc``.

## Dispatch model

The main executable entry point lives in [`c++/pptool.cc`]({{ source_file("c++/pptool.cc") }}).

At startup it performs the shared host-side bootstrap:

1. build a `HostRuntime` from [`c++/host_runtime.hh`]({{ source_file("c++/host_runtime.hh") }})
2. parse command-line options and enable the common runtime policy
3. construct the single `FPGA` object and apply the default clock/PLL startup policy
4. run the startup frequency-meter report that also caches `streamer_clk` in the `FPGA` object
5. dispatch to a tool-specific handler based on the executable name

This means `pptool`, `ppfg`, `ppcounter`, `ppdelay`, and several other commands are all different front doors into the same host-side runtime.

The clock-selection and PLL choices consumed during startup are resolved by [`c++/options.hh`]({{ source_file("c++/options.hh") }}), applied by [`c++/startup.hh`]({{ source_file("c++/startup.hh") }}), and packaged together by [`c++/host_runtime.hh`]({{ source_file("c++/host_runtime.hh") }}), which keeps startup behavior aligned across the main executables.

By default, startup does not pulse the FPGA reset manager. Use `-reset_FPGA` or set `PP_RESET_FPGA` when a process-startup FPGA S2F reset is required. `ppreset` is the exception: it always performs that reset during startup.

At process exit, HostRuntime-based C++ tools warn about option-looking command-line tokens that were never consumed by the parser. This is a non-fatal diagnostic intended to catch stale or misspelled options without changing existing command return codes.

## Advanced/debug knobs

These switches are intended for bring-up, diagnostics, and controlled test environments. They can
change timing or mask hardware-check failures, so avoid them in normal automated pass/fail runs
unless the caller deliberately wants that behavior.

* `-exit_delay T` or `PP_EXIT_DELAY`: sleep for the requested time after the command has finished its main work, keeping outputs and status stable briefly before process exit.
* `-rbmode 1` or `PP_RBMODE=1`: select the supported readback valid/clock mode. Other readback strobe modes are not available in the current build.
* `-stop_on_buffer_error` or `-sobe`: request streamer stop-on-buffer-error behavior when a command constructs the shared streamer helper. With the default policy, underrun is still latched in `buffer_error` and playback may continue until a terminator, but `done` remains clean-completion-only.
* `-pp_ignore_qout_final` or `PP_IGNORE_QOUT_FINAL`: do not fail a streaming command solely because the final observed `qout` differs from the inferred expected final value.
* `-ignore_rb_error_if_crc_ok` or `PP_IGNORE_RB_ERROR_IF_CRC_OK`: clear only the readback-check error contribution when the readback path reported an error but the streamer and readback CRC values still match; final `qout` and FIFO check failures are still reported.
* `-server`: start the optional debug line server while the selected command runs. It binds to `-ip` (default `0.0.0.0`) and `-port` (default `5555`), or UDP when `-udp` is present. A `0.0.0.0` bind prints the external-interface warning and waits one second before accepting traffic; use `-ip 127.0.0.1` for local-only access.

The command catalog itself is declared in [`c++/pptool_commands.hh`]({{ source_file("c++/pptool_commands.hh") }}) and implemented mainly in:

* [`c++/pptool_streaming.cc`]({{ source_file("c++/pptool_streaming.cc") }})
* [`c++/pptool_measurement.cc`]({{ source_file("c++/pptool_measurement.cc") }})
* [`c++/ppscpi.cc`]({{ source_file("c++/ppscpi.cc") }})

## Command families

Broadly, the commands fall into these groups:

* streaming/output generation - `ppfg`, `ppdelay`, `ppplay`, `ppvcd`, `pptest`, `ppdmatest`, `ppmstest`
* trigger/output debugging - `pptrig`, `ppqout`, `ppaux`, `ppreset`
* measurement/readback - `ppread`, `ppcounter`, `ppts`, `ppfreq`, `pptemp`, `ppgpsdo`
* onboarding/smoke test - `pphelloworld`
* remote-control servers - `ppscpi`, `ppwebgui`
* standalone clock helper - `pllcalc`

Many streaming-oriented commands share the same lower-level execution path in [`ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }}), which handles sequence transmission, trigger control, optional readback verification, completion checks, FIFO accounting, and CRC comparison.

## How to extend it

To add a new command mode:

1. declare a new `pp...` handler in [`c++/pptool_commands.hh`]({{ source_file("c++/pptool_commands.hh") }})
2. implement the handler in the appropriate [`pptool_*.cc`]({{ source_file("c++/") }}) file
3. register it in the dispatch table in [`c++/pptool.cc`]({{ source_file("c++/pptool.cc") }})
4. add user-facing docs under `docs/docs/`

This keeps the high-level user interface stable while letting the internal implementation evolve.

## Related docs

* [pptest](pptest.md)
* [Return codes](return_codes.md)
* [ppmstest](pptest.md#ppmstest)
* [ppdmatest](pptest.md#ppdmatest)
* [ppfg](ppfg.md)
* [ppplay](ppplay.md)
* [ppdelay](ppdelay.md)
* [ppreset](ppreset.md)
* [pptrig](pptrig.md)
* [ppqout](ppqout.md)
* [ppaux](ppaux.md)
* [ppcounter](ppcounter.md)
* [ppts](ppts.md)
* [ppgpsdo](ppgpsdo.md)
* [pptemp](pptemp.md)
* [ppfreq](ppfreq.md)
* [ppvcd](ppvcd.md) - VCD entry point for `ppplay`
* [pphelloworld](pphelloworld.md)

Related non-symlink tools:

* [ppscpi](ppscpi.md)
* [ppwebgui](ppwebgui.md)
* [pllcalc](pllcalc.md)

See also:

* [C++ API](cpp.md)
* [`c++/README.md`]({{ source_file("c++/README.md") }})

## Bash completion on the board

PulsePins ships a Bash-completion source file for the `pptool` command family at [`contrib/completions/pulsepins.bash`]({{ source_file("contrib/completions/pulsepins.bash") }}).

It is already installed on the prepackaged quick-start images distributed for SD-card use, so users booting those images do not need to install it manually.

To install it on a live board, run:

```bash
sudo ./scripts/install_bash_completion.sh
```

The installer copies the completion file to `/etc/profile.d/pulsepins-completion.sh`, which is sourced automatically by the board's `/etc/profile` for login shells.

After installation, either log out and back in or reload the shell profile manually:

```bash
source /etc/profile
```

The first version covers `pptool` and the `pp...` symlink commands, with static completion for common options and file-path completion for arguments such as `ppplay -file`.
