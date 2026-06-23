// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Helpers for reading temperatures from the MCP9808 sensor over I2C.

#pragma once

#include <array>
#include <cerrno>
#include <cstdint>
#include <ctime>

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "I2C.hh"

static constexpr uint8_t REG_AMBIENT_TEMP = 0x05;

struct Args {
  int bus = 1;
  int addr = 0x18;
  double delay = 1.0;
  uint64_t count = 0;          // 0 => forever
  bool celsius = true;
  bool fahrenheit = false;
  bool kelvin = false;
  bool timestamp = false;
  bool csv = false;
  bool reopen = false;
  bool quiet_errors = false;
};

static std::string now_iso_utc_seconds() {
  using namespace std::chrono;
  auto now = system_clock::now();
  std::time_t t = system_clock::to_time_t(now);
  std::tm tm_utc{};
  gmtime_r(&t, &tm_utc);
  std::ostringstream oss;
  oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// MCP9808 temperature reading and output formatting.
class MCP9808 {
public:
  MCP9808(int bus, int addr7, bool reopen_each_sample)
    : bus_(bus), addr_(addr7), reopen_(reopen_each_sample) {
      if (!reopen_) dev_.emplace(bus_);
    }

    double read_temp_c() {
      if (reopen_) {
        I2CDevice d(bus_);
        return read_from_device_(d);
      }
      return read_from_device_(*dev_);
    }

  // Formatting moved here, per request.
  static void print_csv_header(const Args& args, std::ostream& os) {
    if (!args.csv) return;
    os << "timestamp";
    if (args.celsius) os << ",temp_c";
    if (args.fahrenheit) os << ",temp_f";
    if (args.kelvin) os << ",temp_k";
    os << "\n";
    os.flush();
  }

  static std::string format_line(const Args& args, double t_c) {
    const double t_f = args.fahrenheit ? (t_c * 9.0 / 5.0 + 32.0) : 0.0;
    const double t_k = args.kelvin ? (t_c + 273.15) : 0.0;
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(4);
    if (args.csv) {
      oss << now_iso_utc_seconds();
      if (args.celsius) oss << "," << t_c;
      if (args.fahrenheit) oss << "," << t_f;
      if (args.kelvin) oss << "," << t_k;
      return oss.str();
    }
    if (args.timestamp) oss << now_iso_utc_seconds() << "  ";
    if (args.celsius) oss << t_c << " °C";
    const bool extra_units = args.fahrenheit || args.kelvin;
    if (extra_units) {
      oss << "  (";
      bool need_sep = false;
      if (args.fahrenheit) {
        oss << t_f << " °F";
        need_sep = true;
      }
      if (args.kelvin) {
        if (need_sep) oss << ", ";
        oss << t_k << " K";
      }
      oss << ")";
    }
    return oss.str();
  }

  static void emit_quiet_error_placeholder(const Args& args, std::ostream& out, std::ostream& err, const std::string &detail = "") {
    if (args.csv) {
      out << now_iso_utc_seconds();
      if (args.celsius) out << ",NaN";
      if (args.fahrenheit) out << ",NaN";
      if (args.kelvin) out << ",NaN";
      out << "\n";
      out.flush();
      if (!detail.empty()) {
        err << now_iso_utc_seconds() << "  ERROR: I2C read failed: " << detail << "\n";
        err.flush();
      }
    } else {
      const std::string prefix = args.timestamp ? (now_iso_utc_seconds() + "  ") : "";
      err << prefix << "ERROR: I2C read failed";
      if (!detail.empty())
        err << ": " << detail;
      err << "\n";
      err.flush();
    }
  }

private:
  static double decode_temp_c_(uint8_t msb, uint8_t lsb) {
    const uint16_t raw = (static_cast<uint16_t>(msb) << 8) | static_cast<uint16_t>(lsb);
    const bool sign = (raw & 0x1000u) != 0;     // bit 12
    const uint16_t temp_raw = (raw & 0x0FFFu);  // lower 13 bits effectively used in the Python version
    double temp_c = static_cast<double>(temp_raw) * 0.0625;
    if (sign) temp_c -= 256.0;
    return temp_c;
  }

  double read_from_device_(I2CDevice& d) {
    const auto bytes = d.read_reg2(static_cast<uint8_t>(addr_), REG_AMBIENT_TEMP);
    return decode_temp_c_(bytes[0], bytes[1]);
  }

  int bus_;
  int addr_;
  bool reopen_;
  std::optional<I2CDevice> dev_;
};
