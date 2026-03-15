#pragma once

#include <vector>
#include <cmath>
#include "tidbit.hh"
#include "freqfmt.hh"

class freq_meter {
 private:
   using Ticks = uint32_t; // width of all counters (incl. gate length)
   double nominal_cnt_clk_freq = 50000000; // Hz
   double correction_factor = 1.0; // true_cnt_clk_freq/nominal_cnt_clk_freq
   static constexpr uint32_t default_gate_len =  500*1000; // 10ms, assuming 50MHz clock
   Ticks gate_len;
   loc lctl;
   loc lgate_len;
   loc ln_ch;
   std::vector<loc> lresult;
   int n_ch;
   bool verbose;

 public:
   freq_meter(const mm &dev, const std::uintptr_t base, const bool _verbose = true) :
     lctl(dev.get_loc(base, 0)),
     lgate_len(dev.get_loc(base, 4)),
     ln_ch(dev.get_loc(base, 8)),
     verbose(_verbose)
   {
     n_ch = ln_ch.read();
     assert(1 <= n_ch && n_ch <= 4);
     if (verbose)
       std::cout << "freq_meter: n_ch=" << std::dec << n_ch << std::endl;
     set_gate_len(default_gate_len);
     lresult.reserve(n_ch);
     for (int i = 0; i < n_ch; i++)
       lresult.push_back(dev.get_loc(base, 0x10 + 4*i));
   }

   void set_gate_len(Ticks new_gate_len) {
     gate_len = new_gate_len;
     lgate_len.write(gate_len);
     lctl.write(2); // clear
     lctl.write(1); // enable
     if (verbose)
       std::cout << "freq_meter: gate_len=" << std::dec << gate_len << std::endl;
   }

   auto get_gate_len() const {
     return gate_len;
   }

   void set_gate_time(double t) { // t in seconds
     set_gate_len(t*nominal_cnt_clk_freq);
   }

   Ticks read(const int i) {
     return lresult[i].read();
   }

   double read_freq(const int i) {
     const auto t = read(i);
     return double(t)/gate_len * nominal_cnt_clk_freq * correction_factor;
   }

   // Formated output with the number of decimals consistent with the frequency resultion set by gate time
   std::string read_freq_str(const int i) {
     const auto digits = std::ceil(std::log10(gate_len));
     return freqfmt::format_frequency(read_freq(i), digits, '\'');
   }

   void set_nominal_cnt_clk_freq(double f) {
     nominal_cnt_clk_freq = f;
   }

   void set_correction_factor(double f) {
     correction_factor = f;
   }

   auto get_n_ch() const {
     return n_ch;
   }

   void wait_one_gate_time() const {
     usleep( double(gate_len)/nominal_cnt_clk_freq * 1000*1000 );
   }
};

constexpr int METER_EXT_CLK = 0;
constexpr int METER_INT_CLK = 1;
constexpr int METER_STREAMER_CLK = 2;
constexpr int METER_CORE_CLK = 3;

class pp_freq_meter {
 private:
   const InputParser &input;
   FPGA &fpga;

   static constexpr auto cli_rescale = "-freq_rescale";
   static constexpr auto env_rescale = "PP_FREQ_RESCALE";

 public:
   freq_meter meter;

   // if wait=true, wait until the first reading becomes valid
   pp_freq_meter(const InputParser &_input, FPGA &_fpga, const bool wait = true) :
     input(_input),
     fpga(_fpga),
     meter(fpga.dev_h2f, FREQ_METER_0_BASE) {
       if (envVarExists(env_rescale) || input.exists(cli_rescale))
         meter.set_correction_factor(parse_double(input, cli_rescale, get_env(env_rescale)));
       const auto n_ch = meter.get_n_ch();
       assert(n_ch == 4);
       if (wait)
         meter.wait_one_gate_time();
       fpga.set_streamer_clk(meter.read_freq(METER_STREAMER_CLK));
     }

   void report() {
     std::cout << "ext_clk      " << meter.read_freq_str(METER_EXT_CLK) << std::endl;
     std::cout << "int_clk      " << meter.read_freq_str(METER_INT_CLK) << std::endl;
     std::cout << "streamer_clk " << meter.read_freq_str(METER_STREAMER_CLK) << std::endl;
     std::cout << "core_clk     " << meter.read_freq_str(METER_CORE_CLK) << std::endl;
   }
};
