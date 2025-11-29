# Implementation details

## Streamer core

### Configuration file

`IP/streamer/config.vh` - this file defines the bit widths of various registers (data, counter, control)

`WIDTH_COUNTER`: number of bits of the integer variable that specifies the length of runs in run-length encoding. Defaults to 32. A version with 64-bit counters is available
for high-speed systems and for allowing extremely long delays.

`WIDTH_DATA`: number of bits of data. Defaults to 32. A 64-bit version is also available.

`WIDTH_CONTROL`: number of bits in the register that controls the streamer core. Fixed to 32.

`WIDTH_TRIGGER`: number of trigger inputs. Defaults to 8. Both the trigger pattern and the trigger mask are of this length.

`WIDTH_TRIGGER_CONTROL`: number of bits in the trigger control register. Fixed to 16.

`P_FIFO_TRIGGER`: exponent p that fixes the length 2^p of the FIFO buffer for trigger conditions (pattern and mask pairs) for
complex serial trigger sequences.

`P_FIFO_IN`: exponent p that fixes the length 2^p of the input FIFO buffer for (counter,data,control) triplets
received from the software. The buffer should be large enough so that it never underflows during the streaming process.
Underflows are possible if there are long sequences of elements with very short lengths. Used in `input_fifo.v`.

`P_FIFO_OUT`: exponent p that fixes the legnth 2^p of the output FIFO buffer for the output data. This size is not
critical. Used in `output_fifo.v`.

`MEMORY_POSITIONS`: number of elements that can be stored in the preprocessor for replays


### Streamer control registers

Note that the addressed are given in 32-bit word steps (multiply by 4 to get an actual address in units of bytes).

Write:

| Name          | Address     | Description      |
| ------------- | ----------- | ---------------- |
| IF_CTRL       | b000        | Control register |
| INIT_VAL      | b100        | Initial value    |
| QOUT_OVERRIDE | b110        | Override value for the output register |
| GATING_W      | b111        | Gating control resiter (mask, gate_in enable, gating enable) |

Read:


| Name          | Address     | Description                        |
| -------       | ----------- | ---------------------------------- |
| IF_STATUS     | b000        | Status register                    |
| QOUT          | b100        | Current value at the output        |
| QOUT_STREAMER | b010        | Current value at the streamer port |
| EXT_TRIG_IN   | b001        | External trigger inputs            |
| EXT_TRIG_CTRL | b011        | External trigger control signals   |
| GATRING_R     | b111        | Gating signals and control         |



Signals in the status (read) register

| Bit   | Name                 | Description |
| ----- | ----                 | ----------- |
| 0     | buffer_error         | Error occured during streaming    |
| 1     | done                 | Streaming completed without error |
| 2     | trigger_activated    | Trigger circuit fired and the data is streaming out |
| 3     | trigger_armed        | Waiting for a trigger event to begin streaming      |


Signals in the control (write) register

| Bit   | Name | Description |
| ----- | ---- | ----------- |
| 0     | stop               | NOT IMPLEMENTED                                                                      |
| 1     | trigger_force_int  | Forces triggering from software (this is or'd with an external force trigger signal) |
| 2     | trigger_enable_int | Enables trigger circuit                                                              |
| 3     | reset_streamer     | Forces reset of the streamer circuit, erases FIFO buffers                            |
| 4     | trigger_reset_int  | Resets the trigger circuit; the trigger will be deactivated and streaming will stop  |
| 5     | qout_select        | Select the output: streamer or override value                                        |


## Triggering mechanism

### Simple trigger

Implemented in ``ip/streamer/and_trigger.v``. The masked bits are compared against the trigger pattern. The high bits
in the mask indicate the active positions in the input port. The trigger is synchronous with the output clock, i.e.,
the trigger inputs are compared against the pattern when the clock is asserted.

The trigger is sensitive to the inputs only if the enable signal ``en`` is high. This signal is defined by or'ing together
internal and external trigger enable signals.

The output of the simple trigger is latched. When the output is asserted, it can only be deasserted by a reset
signal.

### Chain trigger

The trigger is implemented as a state machine with the following states:

* ``IDLE``: initial state after a reset, waiting for trigger configuration data
* ``LOAD``: load pattern and mask data from the trigger FIFO
* ``WAIT``:  pattern and mask are defined using the data from the FIFO, trigger is armed, we are waiting for the trigger event
* ``TRIGGERED``: all events were detected (or the trigger has been forced), output from trigger is activated

State changes are synchronous with clock or asynchronous (reset, trigger reset, trigger force).

The trigger can be forced by internal or external force trigger signals even if the trigger is not enabled; this is
because the enable signal is only relevant for the trigger-input detection circuit, force signal overrides that circuit
altogether.
