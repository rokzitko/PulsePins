# Readback

Readback is the name of a run-length encoder core. This core is connected directly to the output of the PulsePins
streamer (``qout``) for verification and testing purposes. It reads streamed symbols and RL-compresses them. The
results can be read through an Avalon-ST interface. PulsePins comes with a suite of testing scripts
that assert correct functionality of the streamer, including various corner cases, by matching the streamed
out data with the reference sequences. The readback mechanism thus provides high assurance.

The acquired sequence uses the same format as the streamer (control, counter, value). The control register is all zeros;
we assume that all data are ``BITLOAD`` updates.

The bus widths in ``ip/rl_encoder_if/rl_config.vh`` need to match those in ``ip/streamer/config.vh``.

## rl_encoder core

Inputs:

 * ``qin``: data signals; connects to ``qout`` of the streamer
 * ``qin_valid``: if 1, qin data is valid
 * ``qin_clk``: data are sampled when qin_clk is asserted
 * ``activated``: enable signal, sampling is only performed when this signal is high

The enable signal `activated` needs to be deasserted at the end of the sequence to indicate that the streaming has
terminated, so that the RL encoder can add the last element to the acquired sequence.

## rl_encoder_if interface

Provides an Avalon-ST interface for reading the acquired sequence and an Avalon-MM interface for control.
The only purpose of the control interface is to reset the reader.

## Software interface

The software interface is provided through class ``readback`` in the header file ``readback.hh``.
The key member functions are:

 * ``reset``: perform the reset of the circuitry
 * ``clear_fifo``: read and discard all elements in the readback queue
 * ``filled``: returns true if there are elements to be read back
 * ``check_fill_status``: provides a report on the Avalon ST reading FIFO
 * ``read``: read a single element
 * ``read_all``: read indefinitely; this function never returns
 * ``check``: perform a comparison between the sequence that is read back and a reference ``Sequence`` object

The function ``check`` returns true if no errors are detected. A timeout argument can be provided; if no new
elements are received during the specified interval, an exception is raised. An exception is also raised if the
reference sequence is exhausted and a new element is received from the encoder. A report is produced when the check
is completed, including the number and ratio of errors, the difference in size (number of elements) and length (number
of periods of strobed data).

## Readback of external signals

If the output enable signal ``oe`` is low, the readback core is reading signals from the external
pins, i.e. the 32 `qout` pins actually act as inputs, and the `streamer_qout_valid` pin starts
acting as the valid signal input port. The [ppread](ppread.md) tool can be used to read the
generated data stream. One application of this tool is to troubleshoot another PulsePins board
by connecting the qout and qout_valid ports together and clocking both devices from the same clock
source.
