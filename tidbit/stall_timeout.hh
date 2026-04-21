// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

class StallTimeout : public std::runtime_error {
public:
  explicit StallTimeout(const std::string &message) : std::runtime_error(message) {}
};

inline constexpr double default_transport_stall_timeout_s = 2.0;
inline constexpr double default_transport_busy_timeout_s = 10.0;

class TimeoutGuard {
public:
  using clock = std::chrono::steady_clock;

private:
  std::string context_;
  double timeout_s_ = 0.0;
  clock::time_point start_ = clock::now();
  clock::time_point last_progress_ = start_;

  [[noreturn]] void throw_timeout(const char *kind, const std::string &details) const {
    std::ostringstream msg;
    msg << context_ << ' ' << kind << " after " << std::fixed << std::setprecision(3)
        << timeout_s_ << "s";
    if (!details.empty())
      msg << ": " << details;
    throw StallTimeout(msg.str());
  }

public:
  explicit TimeoutGuard(std::string context, const double timeout_s) :
    context_(std::move(context)),
    timeout_s_(timeout_s) {}

  bool enabled() const noexcept {
    return timeout_s_ > 0.0;
  }

  void progress() noexcept {
    last_progress_ = clock::now();
  }

  template<typename State>
  bool progress_if_changed(std::optional<State> &previous, const State &current) {
    if (!previous.has_value() || *previous != current) {
      previous = current;
      progress();
      return true;
    }
    return false;
  }

  void throw_if_total_timeout(const std::string &details = {}) const {
    if (!enabled())
      return;
    const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(clock::now() - start_);
    if (elapsed.count() > timeout_s_)
      throw_timeout("timed out", details);
  }

  void throw_if_stalled(const std::string &details = {}) const {
    if (!enabled())
      return;
    const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(clock::now() - last_progress_);
    if (elapsed.count() > timeout_s_)
      throw_timeout("stalled", details);
  }
};
