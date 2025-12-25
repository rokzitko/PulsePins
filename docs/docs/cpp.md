## C++ Application programming interface

### Data types for sequence representation

Class hierarchy for counter value objects:

* ``Counter`` (defined in ``elements.hh``): containers that hold the count value (number of repetitions)

    * ``Strobe``: regular elements that are strobed out
    * ``NoStrobe``: elements that are not strobed out (and the qout_valid signal is deasserted)

Interface of ``Counter`` objects:

* ``count()``: returns the count value
* ``count_str()``: the count value as a string in hexadecimal and decimal form for inspection and troubleshooting
* ``control_bits()``: returns the bits to be or'd in the control register
* ``desc()``: get a descriptive string about the control settings; empty for the default case of ``Strobe``
* ``clone()``: generate a std::share_ptr to a cloned object for use in class compositions (element ``el`` objects, see below)

Update types (``d`` is the input value, ``q_prev`` is the previous output value, ``q`` is the new output value to be streamed out):

* ``LOAD``: loads new data, i.e., verbatim assignment, ``q = d``
* ``SET``: sets bits according to a mask pattern, ``q = q_prev | d``
* ``CLEAR``: clears bits according to a mask pattern, ``q = q_prev & ~d``
* ``FLIP``: flips bits according to a mask pattern, ``q = q_prev ^ d``
* ``NOT``: invert bits, ``q = ~q_prev`` (same as FLIP with all ones)
* ``AND``: bitwise and, ``q = q_prev & d`` (same as CLEAR with inverted values)
* ``OR`` : bitwise or, ``q = q_prev | d`` (same as SET)
* ``XOR`` : bitwise xor, ``q = q_prev ^ d`` (same as FLIP)
* ``XNOR``: bitwise xnor, ``q = q_prev ^~ d`` (equivalence)
* ``SLL`` : shift left logical (fill with zero), ``q = q_prev << d``
* ``SRL`` : shift right logical (fill with zero), ``q = q_prev >> d``

Corresponding class hierarchy:

* ``Value`` (defined in ``elements.hh``): parent class

    * ``BitLoad``
    * ``BitSet``
    * ``BitClear``
    * ``BitFlip``
    * ``BitNot``
    * ``BitAnd``
    * ``BitOr``
    * ``BitXor``
    * ``BitXnor``
    * ``BitSll``
    * ``BitSrl``

Interface of ``Value`` objects:

* ``value()``: returns the stored value
* ``value_str()``: the count value as a string in hexadecimal and decimal form for inspection and troubleshooting
* ``result(value_t v_prev)``: returns the result of the updated value, where ``v_prev`` is the previous value
* ``mode_bits()``: returns the mode bits to be or'd in the control register
* ``desc()``: get a descriptive string describing the update mode
* ``clone()``: generate a std::share_ptr to a cloned object for use in compositions (element ``el`` objects, see below)

An additional class derived from ``Value`` is the class ``TriggerCondition``. It has a bespoke constructor that takes
pattern and mask bits as input and converts that into a ``value_t`` quantity. Furthermore, the ``value_str()`` method
decomposes this value into pattern and mask and represents them as hexadecimal value and bitsets.

Class ``el`` (defined in ``elements.hh``) represents elements of the sequence. There are multiple constructors:

* ``el()``: sequence terminator with a default value of the output bus (0xFFFFFFFF)
* ``el(value_t)``: sequence terminator with a selected final value of the output bus
* ``el(count_t, value_t)``: regular element, ``BitLoad`` update
* ``el(Count, value_t)``: regular element, either ``Strobe`` or ``NoStrobe``, ``BitLoad`` update
* ``el(Count, Value)``: regular element, general interface (BitLoad, BitSet, BitClear, etc.)
* ``el(trigger_t, trigger_t, bool)``: trigger condition element
* ``el(Replay, count_t, value_t)``: replay a subsequence stored in the preprocessor a given number of times
* ``el(Retrig, value_t)``: stop streaming and wait for a retrigger event
* ``el(Random, count_t)``: generate random numbers

The counter and value are internally stored as ``std::shared_ptr`` objects for easy expandability. The
control parameter is stored as a ``control_t`` integer value using bit patters obtained from ``Counter``
and ``Value`` objects using the ``control_bits`` and ``mode_bits`` member functions.

The public interface of ``el`` objects:

* ``control()``, ``count()``, ``value()``: integer representation to be sent to the RLE-decoder core
* ``set_control()``, ``set_count()``, ``set_value()``: low-level update functions
* ``updated_value()``: returns a ``value_t`` value that results from the update; the function takes the previous value
as the input
* ``desc()``: obtain a string representation of the element for inspection and debugging
* ``store(int)``: tag an element for storing in the chosen register of the preprocessor
* ``updated_value(value_t)``: determine what will be the output from the streaming core corresponding to the
given element for a given previous output state
* ``is_regular()``, ``is_trigger()``, ``is_final()``: tests for element type

``el`` objects can be compared and passed to an std stream.

Class ``Sequence`` (defined in ``sequence.hh``) represent a sequence of elements. Internally, this is an overloaded std::deque of ``el`` objects.
Public member functions are:

* ``size()``: total number of elements
* ``data_size()``: number of regular elements
* ``length()``: total duration of the sequence in units of clock periods (length)
* ``dump()``: print out the entire sequence
* ``convert_to_BitLoad()``: return a sequence where all regular (data) elements are converted to BITLOAD updates
* ``merge()``: return a sequence where neighbouring data elements with the same value are merged together

Two sequences can be compared using function ``compare()`` and using ``operator==``.

### Streamer control interface

Class ``streamer_control`` (defined in ``streamer.hh``) is the high-level interface for controlling the RLE-decoder core. Member functions:

* ``status()``: read the status register (buffer error, done, triggered, armed); these are outputs from the
core
* ``get_control()``: returns the control register (stop, internal trigger force, internal trigger enable, reset,
internal trigger reset); these are inputs to the core
* ``get_qout()``: current value at the output (on the "device pins", if there is no postprocessing)
* ``get_qout_streamer()``: current value at the streamer output (which may be overriden, see below)
* ``qout_select()``: determine what data is presented at the output (streamer output or override value)
* ``qout_set()``: override the output with the chosen value
* ``monitor_ext_trig()``: debugging tool for external trigger signals
* ``set_initial_value()``: set the value that is present at the streamer data output before triggering
* ``buffer_error()``: buffer error detected
* ``done()``: sequence decoding completed successfully
* ``status_report()``: dump the status in textual representation
* ``reset_streamer()``: reset the streamer and erase data in all FIFO buffers
* ``reset()``: reset the streamer core (set control register to zero and calls ``reset_streamer``)
* ``trigger_enable()``: enable the trigger circuit; the trigger signals are ignored until this member function is
called (or using an external trigger enable signal)
* ``trigger_force()``: assert the internal trigger force signal
* ``trigger_reset()``: assert the internal trigger reset signal
* ``gating()``: control gatting (control of the streaming using an external output enable signal)

Class ``streamer_fifo`` (defined in ``streamer.hh``) is the high-level interface for spooling the sequence to the RLE-decoder core. Member functions:

* ``out()``: send one element
* ``send_sequence()``: send entire sequence
* ``check_fill_status()``: check FIFO status

Class ``streamer_dma`` (defined in ``streamer.hh``) is the high-level interface for transmitting the sequence to the
RLE-decoder code using direct memory access (DMA). Member functions:

* ``write_element()``: write an element at given memory location
* ``prepare()``: write an entire sequence in the memory buffer
* ``verify()``: verity the correctness of the sequence stored in the memory
* ``transfer()``: perform the transfer
* ``send_sequence()``: high-level function for transmitting a sequence to the streamer via DMA

FPGA (defined in ``fpga.hh``): container for memory-mapped interfacing with the FPGA.
The constructor of this class blinks the on-board diagnostic LED at startup.
If also asserts the output enable ``oe`` signal (by default, can be overriden).

Streamer classes (defined in ``basic_multi_dma.hh``):

* ``basic_streamer``: streamer interface and the associated FIFO buffer
* ``streamer``: high-level interface; adds PLL clock control and FPGA fabric reset interface to the ``basic_streamer``
* ``dma_streamer``: DMA interface
* ``multistreamer``: container for four instances of ``basic_streamer``

Combiner class (defined in ``combiner.hh``): interface for advanced multiplexers.

combiner_qout, qout (defined in ``qout.hh``)

readback (defined in ``readback.hh``)

st_mux (defined in ``st_mux.hh``)

trigger (defined in ``trigger.hh``)
