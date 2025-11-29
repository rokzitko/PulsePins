// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Simple function generator

#pragma once

#include <iostream>
#include <utility>
#include <stdexcept>
#include <cmath> // round
#include <limits>

#include "parser.hh"
#include "elements.hh"
#include "sequence.hh"

// Determine the requested period from -period or -freq settinh
double parse_period(const InputParser &input)
{
  double period_req = 0;
  if (input.exists("-period") && input.exists("-freq"))
    throw std::runtime_error("Specify either -period or -freq, not both!");
  if (input.exists("-period"))
    period_req = parse_time(input, "-period", "0");
  if (input.exists("-freq"))
    period_req = 1.0/parse_frequency(input, "-freq", "0");
  return period_req;
}
constexpr double default_output_clk = 100 * 1000 * 1000.0; // default output clock frequency [Hz]

std::pair<uint64_t, uint64_t> calc_pos_neg(const double period_req,
                                           const double duty,
                                           const double output_clk = default_output_clk)
{
  if (period_req < 0.0)
    throw std::runtime_error("Period/frequency must be a positive quantity.");
  if (duty < 0.0 || duty > 100.0)
    throw std::runtime_error("Duty cycle must be a value between 0 and 100 (percent).");
  const double output_clk_period = 1.0/output_clk;
  uint64_t nr = round(period_req/output_clk_period);
  nr = (nr > 0 ? nr : 2); // the smallest sensible value is 2
  double period_resulting = nr*output_clk_period;
  std::cout << "period (requested)=" << pretty_time(period_req) << " (resulting)="
    << pretty_time(period_resulting) << " nr=" << std::dec << nr << std::endl;
  std::cout << "frequency (requested)=" << pretty_frequency(1.0/period_req) << " (resulting)="
    << pretty_frequency(1.0/period_resulting) << std::endl;
  uint64_t nr_pos = duty/100.0*nr;
  nr_pos = (nr_pos > 0 ? nr_pos : 1); // the smallest sensible value is 1
  uint64_t nr_neg = nr-nr_pos;
  if (nr_neg == 0) {
    nr_neg = 1;
    nr_pos = nr_pos-1;
  }
  assert(nr_pos > 0 && nr_neg > 0);
  if (nr_pos > std::numeric_limits<count_t>::max())
    throw std::runtime_error("Number of periods too large for the bit-width of the counter (nr_pos).");
  if (nr_neg > std::numeric_limits<count_t>::max())
    throw std::runtime_error("Number of periods too large for the bit-width of the counter (nr_neg).");
  std::cout << "duty=" << duty << " time_pos=" << pretty_time(nr_pos*output_clk_period)
    << " time_neg=" << pretty_time(nr_neg*output_clk_period) << std::endl;
  std::cout << "nr_pos=" << std::dec << nr_pos << " (0x" << std::hex << nr_pos << ")" <<
    " nr_neg=" << std::dec << nr_neg << " (0x" << std::hex << nr_neg << ")" << std::endl;
  return {nr_pos, nr_neg};
}

uint64_t calc_delay(const double delay, const double output_clk = default_output_clk)
{
  const double output_clk_period = 1.0/output_clk;
  uint64_t nr_delay = round(delay/output_clk_period);
  double delay_resulting = nr_delay*output_clk_period;
  std::cout << "delay (requested)=" << pretty_time(delay) << " (resulting)="
    << pretty_time(delay_resulting) << " nr_delay=" << std::dec << nr_delay << std::endl;
  if (nr_delay > std::numeric_limits<count_t>::max())
    throw std::runtime_error("Number of periods too large for the bit-width of the counter (nr_delay).");
  return nr_delay;
}

auto seq_continuous(trigger_t p, trigger_t m, count_t nr_delay, count_t nr_pos, count_t nr_neg, value_t v1, value_t v0, bool start0 = false)
{
  Sequence elements;
  elements.push_back(el(p, m, true));
  if (start0) {
    elements.push_back(el(nr_neg, v0).store(0));
    elements.push_back(el(nr_pos, v1).store(1));
  } else {
    elements.push_back(el(nr_pos, v1).store(0));
    elements.push_back(el(nr_neg, v0).store(1));
  }
  if (nr_delay>0)
    elements.push_back(el(nr_delay, v0));
  elements.push_back(el(Replay{}, 0, 2)); // 0 = repeat indefinitely
  elements.push_back(el());
  return elements;
}

auto seq_burst(trigger_t p, trigger_t m, count_t nr_delay, count_t nr_pos, count_t nr_neg, value_t v1, value_t v0, int rep, bool start0, value_t final)
{
  Sequence elements;
  elements.push_back(el(p, m, true));
  if (start0) {
    elements.push_back(el(nr_neg, v0).store(0));
    elements.push_back(el(nr_pos, v1).store(1));
  } else {
    elements.push_back(el(nr_pos, v1).store(0));
    elements.push_back(el(nr_neg, v0).store(1));
  }
  if (nr_delay>0)
    elements.push_back(el(nr_delay, v0));
  elements.push_back(el(Replay{}, rep, 2));
  elements.push_back(el(NoStrobe{1}, final)); // set final value
  return elements;
}

auto seq_once(trigger_t p, trigger_t m, count_t nr_delay, count_t nr_pos, count_t nr_neg, value_t v1, value_t v0, value_t final)
{
  Sequence elements;
  elements.push_back(el(p, m, true));
  if (nr_delay>0)
    elements.push_back(el(nr_delay, v0));
  elements.push_back(el(nr_pos, v1));
  elements.push_back(el(nr_neg, v0));
  elements.push_back(el(final));
  return elements;
}

// Map target servo angle (degrees) to PWM frequency and duty cycle
// angle_min, angle_max define the servo range (deg)
// pulse_min, pulse_max define the pulse width range (s)
std::pair<double, double> servo_pwm_params(double angle, 
                                           double angle_min = 0.0, 
                                           double angle_max = 180.0,
                                           double pulse_min = 1e-3,    // 1.0 ms
                                           double pulse_max = 2e-3)    // 2.0 ms
{
  constexpr double period = 20e-3;   // 20 ms
  constexpr double frequency = 1.0 / period; // 50 Hz

  // Clamp input angle
  if (angle < angle_min) angle = angle_min;
  if (angle > angle_max) angle = angle_max;

  // Linear interpolation
  double t = (angle - angle_min) / (angle_max - angle_min);
  double pulse_width = pulse_min + t * (pulse_max - pulse_min);

  // Duty cycle = pulse / period
  double duty_cycle = pulse_width / period;

  return {frequency, duty_cycle};
}
