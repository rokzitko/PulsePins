RLE stream decoder and triggering circuits.

* st_interface.sv - high-level interface
* streamer.sv - streamer core
* input_fifo.sv - input FIFO buffer (holds input elements to be decoded)
* preprocessor.sv - second-level run-length decoder
* rl_decoder.sv - RL decoder
* output_fifo.sv - output FIFO buffer (holds output data to be streamed out)
* and_trigger.sv - single-stage trigger
* chain_trigger.sv - multi-stage trigger
* prng.sv - pseudorandom number generator (xoroshiro128+)
