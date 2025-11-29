## ppqout

Testing tool for output combiner.

Command line parameters:

* ``-i1``, ``-i2``, ``-i3``, ``-i4``: initial values for streamers
* ``-q1``, ``-q2``, ``-q3``, ``-q4``: force values on individual streamer outputs
* ``-out_sel1``: streamer 1
* ``-out_sel2``: streamer 2
* ``-out_sel3``: streamer 3
* ``-out_sel4``: streamer 4
* ``-out_and``: bitwise AND, conjuction
* ``-out_or``: bitwise OR, inclusive disjunction
* ``-out_xor``: bitwise XOR, exclusive disjunction, odd-parity
* ``-out_xnor``: bitwise XNOR, even-parity
* ``-out_majority``: bitwise majority
* ``-out_block8``: 8 bits per streamer (LSB from first streamer)
* ``-out_block16``: 16 bits from first streamer, 16 bits from second streamer
* ``-out_sum12``: algebraic sum of first two streamers
* ``-out_sum1234``: algebraic total
* ``-out_dum12``: difference 2-1
* ``-invert1,2,3,4``: inversion of bits for each port (bitwise XOR)
* ``-mask1,2,3,4``: masking of bits for each port (bitwise AND)
* ``-force1,2,3,4``: override of the output file for each port
* ``-inver_out``, ``-mask_out``, ``-force_out``: inversion, masking and override for the output port
* ``-report_pre``: report initial settings at the program start
* ``-report_post``: report final settings after configuring the combiner
* ``-self_test``: perform basic internal tests
* ``-test <n>``: perform intensive self tests; n is the number of repetitions
