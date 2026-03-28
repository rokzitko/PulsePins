#pragma once

#include <cstdint>
#include <chrono>
#include <exception>

#include "fifo.hh"
#include "pio.hh"

constexpr int TS_SEL_PULSE_1MS = 7;
constexpr int TS_SEL_PULSE_10MS = 6;
constexpr int TS_SEL_PULSE_100MS = 5;
constexpr int TS_SEL_PULSE_1S = 4;
constexpr int TS_SEL_PIO_AUX_IN0 = 3;
constexpr int TS_SEL_EXT_TRIGGER_IN0 = 2;
constexpr int TS_SEL_STREAMER_TRIGGER_IN0 = 1;
constexpr int TS_SEL_STREAMER_TRIGGER_ACTIVATED = 0;

constexpr int CFG_TS_SEL_PPS = 1;
constexpr int CFG_TS_SEL_SIGA_MUX_OFFSET = 2;

inline std::string sel_str(int sel)
{
  switch (sel) {
  case TS_SEL_PULSE_1MS:
    return "pulse 1ms";
  case TS_SEL_PULSE_10MS:
    return "pulse 10ms";
  case TS_SEL_PULSE_100MS:
    return "pulse 100ms";
  case TS_SEL_PULSE_1S:
    return "pulse 1s";
  case TS_SEL_PIO_AUX_IN0:
    return "aux in 0";
  case TS_SEL_EXT_TRIGGER_IN0:
    return "ext trig 0";
  case TS_SEL_STREAMER_TRIGGER_IN0:
    return "trigger in0";
  case  TS_SEL_STREAMER_TRIGGER_ACTIVATED:
    return "trigger activated";
  default:
    return "INVALID";
  }
}

class timestamp {
 private:
   fifo ff;
   fifo ffA;
   pio_out_bits pio_cfg;

   uint32_t read_one() {
     while (!filled()) {}
     return ff.read();
   }
   uint32_t read_oneA() {
     while (!filledA()) {}
     return ffA.read();
   }

 public:
   timestamp(mm &dev_h2f,
             mm &dev_lw,
             const std::uintptr_t base, const std::uintptr_t in_csr_base,
             const std::uintptr_t baseA, const std::uintptr_t in_csr_baseA,
             const std::uintptr_t pio_cfg_base) :
     ff(dev_h2f, base, in_csr_base),
     ffA(dev_h2f, baseA, in_csr_baseA),
     pio_cfg(dev_lw, pio_cfg_base)
     {
       clear_fifo();
       clear_fifoA();
     }

   void sel_pps_xtal() {
     pio_cfg.clear(1 << CFG_TS_SEL_PPS);
   }

   void sel_pps_in() {
     pio_cfg.set(1 << CFG_TS_SEL_PPS);
   }

   void selA(const int i) {
     assert(0 <= i && i <= 7);
     pio_cfg.clear((1 << 2) + (1 << 3) + (1 << 4));
     pio_cfg.set(i << 2);
   }

   std::string get_cfg() {
     const auto cfg = pio_cfg.read();
     std::stringstream ss;
     ss << ((cfg & (1 << CFG_TS_SEL_PPS)) ? "PPS_IN" : "PPS_XTAL");
     const int i = (cfg >> 2) & 0x7;
     ss << " " << "sel=[" << sel_str(i) << "]";
     return ss.str();
   }

   // Returns true if there are elements to be read back.
   bool filled() {
     return ff.fill() > 0;
   }
   bool filledA() {
     return ffA.fill() > 0;
   }

   void clear_fifo() {
     while (filled())
       ff.read(); // ignore return value
   }
   void clear_fifoA() {
     while (filledA())
       ffA.read(); // ignore return value
   }

   uint64_t read() {
     return (uint64_t(read_one()) << 32) + read_one();
   }
   uint64_t readA() {
     return (uint64_t(read_oneA()) << 32) + read_oneA();
   }

   uint64_t read_with_timeout(const double timeout = 2.0) {
     std::chrono::steady_clock::time_point initial_time = std::chrono::steady_clock::now();
     while (ff.fill() < 2) {
       auto now = std::chrono::steady_clock::now();
       auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
       if (timeout > 0.0 && elapsed.count() > abs(timeout))
         throw std::runtime_error("Timeout.");
       usleep(100); // don't hose CPU in poll loop
     }
     return read();
   }

   uint64_t readA_with_timeout(const double timeout = 2.0) {
     std::chrono::steady_clock::time_point initial_time = std::chrono::steady_clock::now();
     while (ffA.fill() < 2) {
       auto now = std::chrono::steady_clock::now();
       auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
       if (timeout > 0.0 && elapsed.count() > abs(timeout))
         throw std::runtime_error("Timeout.");
       usleep(100); // don't hose CPU in poll loop
     }
     return readA();
   }
};
