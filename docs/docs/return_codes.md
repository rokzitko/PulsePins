# Return Codes

PulsePins command-line tools use the shared return-code constants defined in [`c++/definitions.hh`]({{ source_file("c++/definitions.hh") }}).

`0` means success. Non-zero values are bit flags, so a command can report more than one failure condition at the same time. For example, `68` means `RC_TIMEOUT | RC_ERROR_CHECK`.

| Value | Constant | Meaning |
|---:|---|---|
| `0` | `RC_OK` | Success. |
| `1` | `RC_EXCEPTION` | An exception or other internal fatal error occurred. |
| `2` | `RC_INVALID_ARG` | The command, subcommand, test number, or argument set was invalid. |
| `4` | `RC_ERROR_CHECK` | A data/check invariant failed, such as readback comparison, final `qout`, FIFO accounting, counter self-check, or combiner self-test. |
| `8` | `RC_ERROR_CRC_MISMATCH` | The streamer-side CRC and readback CRC did not match. |
| `16` | `RC_ERROR_BUFFER_ERROR` | The streamer reported an output-side buffer or underrun error. |
| `32` | `RC_ERROR_OVERFLOW` | A streamer input FIFO or readback path overflow was detected. |
| `64` | `RC_TIMEOUT` | A bounded wait timed out, such as transport queueing, readback, streamer completion, or timestamp reads. |

Streaming-oriented commands use the shared workflow in [`c++/ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }}), so readback, CRC, FIFO, final-output, overflow, buffer, and timeout failures can be combined in one return code.

Streamer playback completion waits have an internal timeout. The wait ends on clean `done` or a latched `buffer_error`; if neither status appears within the internal limit, the command sets `RC_TIMEOUT`, reports `timed out waiting for streamer completion (10 s internal limit)`, aborts the active streamer by asserting `stop` and pulsing streamer reset, and skips the normal post-completion checks.

Readback timeouts that happen during `-check` set both `RC_TIMEOUT` and `RC_ERROR_CHECK`.

The `pptool` family supports `-ignore-errors`, which resets a non-zero return code to `RC_OK` after printing a warning. Use this only when the caller intentionally wants to treat hardware/test failures as a successful process exit.
