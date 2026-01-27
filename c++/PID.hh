#pragma once
#include <chrono>
#include <algorithm>
#include <limits>

inline double clamp(double x, double lo, double hi) {
  return std::max(lo, std::min(x, hi));
}

class PID {
public:
   using Clock = std::chrono::steady_clock;

   PID(double kp, double ki, double kd = 0.0,
       double umin = -std::numeric_limits<double>::infinity(),
       double umax = +std::numeric_limits<double>::infinity())
     : kp_(kp), ki_(ki), kd_(kd), umin_(umin), umax_(umax) {}

   void setGains(double kp, double ki, double kd = 0.0) {
     kp_ = kp;
     ki_ = ki;
     kd_ = kd;
   }

   void setLimits(double umin, double umax) {
     umin_ = umin;
     umax_ = umax;
   }

   void reset(double integrator = 0.0) {
     i_ = integrator;
     e_prev_ = 0.0;
     has_t_ = false;
   }

   // Call whenever you have a new error sample. Returns control output.
   double update(double e) {
     const auto now = Clock::now();
     // First call: no dt available yet
     if (!has_t_) {
       has_t_ = true;
       t_prev_ = now;
       e_prev_ = e;
       control = clamp(kp_ * e + i_, umin_, umax_);
       return control;
     }
     auto dt_us = std::chrono::duration_cast<std::chrono::microseconds>(now - t_prev_).count();
     const double dt = 1e-6 * static_cast<double>(dt_us); // seconds
     t_prev_ = now;
     // Derivative on error (default kd_=0; keep it that way unless you have a reason)
     const double de = (e - e_prev_) / dt;
     const double p  = kp_ * e;
     const double d  = kd_ * de;
     // Tentative integrate
     const double i_candidate = i_ + ki_ * e * dt;
     const double u_unsat = p + i_candidate + d;
     const double u_sat   = clamp(u_unsat, umin_, umax_);
     // Anti-windup (conditional integration):
     // If saturated and the current error would drive further into saturation, do not integrate.
     if (u_unsat != u_sat) {
       const bool sat_hi = (u_sat >= umax_);
       const bool sat_lo = (u_sat <= umin_);
       const bool push_hi = (e > 0.0);
       const bool push_lo = (e < 0.0);
       if (!((sat_hi && push_hi) || (sat_lo && push_lo))) {
         i_ = i_candidate;
       }
       // else: keep i_ unchanged
     } else {
       i_ = i_candidate;
     }
     e_prev_ = e;
     control = clamp(p + i_ + d, umin_, umax_);
     return control;
   }

   double getControl() const {
     return control;
   }

private:
   double kp_, ki_, kd_;
   double umin_, umax_;
   double i_{0.0};
   double e_prev_{0.0};
   bool has_t_{false};
   Clock::time_point t_prev_{};
   double control{0.0};
};
