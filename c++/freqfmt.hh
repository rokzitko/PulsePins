// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Frequency formatting helpers with SI scaling and grouped digits.

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <algorithm>

namespace freqfmt {

// group "1234567" -> "1_234_567" (from right)
inline std::string group3_int(const std::string& digits, char sep) {
    if (sep == '\0' || digits.size() <= 3) return digits;
    std::string out;
    out.reserve(digits.size() + (digits.size() - 1) / 3);

    std::size_t n = digits.size();
    std::size_t first = n % 3;
    if (first == 0) first = 3;

    out.append(digits, 0, first);
    for (std::size_t i = first; i < n; i += 3) {
        out.push_back(sep);
        out.append(digits, i, 3);
    }
    return out;
}

// group fractional "1234567" -> "123_456_7" (from left, after decimal point)
inline std::string group3_frac(const std::string& digits, char sep) {
    if (sep == '\0' || digits.size() <= 3) return digits;
    std::string out;
    out.reserve(digits.size() + (digits.size() - 1) / 3);

    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i && (i % 3 == 0)) out.push_back(sep);
        out.push_back(digits[i]);
    }
    return out;
}

// round half-away-from-zero to `decimals` places
inline double round_to_decimals(double x, int decimals) {
    if (decimals <= 0) {
        return std::copysign(std::floor(std::fabs(x) + 0.5), x);
    }
    const double p = std::pow(10.0, decimals);
    return std::copysign(std::floor(std::fabs(x) * p + 0.5) / p, x);
}

inline std::string u64_to_string(std::uint64_t v) {
    if (v == 0) return "0";
    std::string s;
    while (v) { s.push_back(char('0' + (v % 10))); v /= 10; }
    std::reverse(s.begin(), s.end());
    return s;
}

// Format x >= 0 as fixed with `decimals`, then trim trailing zeros from fractional part.
inline std::string fixed_trim(double x, int decimals) {
    x = round_to_decimals(x, decimals);

    double ip_d = 0.0;
    [[maybe_unused]] double fp = std::modf(x, &ip_d);

    std::string ip;
    if (ip_d <= static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        ip = u64_to_string(static_cast<std::uint64_t>(ip_d));
    } else {
        // Very large fallback (unlikely for frequency formatting)
        ip = std::to_string(static_cast<long double>(ip_d));
        auto p = ip.find('.');
        if (p != std::string::npos) ip.erase(p);
    }

    if (decimals <= 0) return ip;

    // Recompute fractional digits from rounded x to avoid drift
    double ip2_d;
    double fp2 = std::modf(x, &ip2_d);
    (void)ip2_d;

    const double scale = std::pow(10.0, decimals);
    long long frac_int = static_cast<long long>(std::llround(fp2 * scale));
    if (frac_int < 0) frac_int = -frac_int;
    if (frac_int >= static_cast<long long>(scale)) frac_int = static_cast<long long>(scale - 1);

    std::string frac(decimals, '0');
    for (int i = decimals - 1; i >= 0; --i) {
        frac[i] = char('0' + (frac_int % 10));
        frac_int /= 10;
    }

    while (!frac.empty() && frac.back() == '0') frac.pop_back();
    if (frac.empty()) return ip;
    return ip + "." + frac;
}

inline std::string fixed(double x, int decimals, bool trim_trailing_zeros) {
    x = round_to_decimals(x, decimals);

    double ip_d = 0.0;
    std::modf(x, &ip_d);

    std::string ip;
    if (ip_d <= static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        ip = u64_to_string(static_cast<std::uint64_t>(ip_d));
    } else {
        ip = std::to_string(static_cast<long double>(ip_d));
        auto p = ip.find('.');
        if (p != std::string::npos) ip.erase(p);
    }

    if (decimals <= 0) return ip;

    double ip2_d;
    double fp2 = std::modf(x, &ip2_d);
    (void)ip2_d;

    const double scale = std::pow(10.0, decimals);
    long long frac_int = static_cast<long long>(std::llround(fp2 * scale));
    if (frac_int < 0) frac_int = -frac_int;
    if (frac_int >= static_cast<long long>(scale)) frac_int = static_cast<long long>(scale - 1);

    std::string frac(decimals, '0');
    for (int i = decimals - 1; i >= 0; --i) {
        frac[i] = char('0' + (frac_int % 10));
        frac_int /= 10;
    }

    if (trim_trailing_zeros) {
        while (!frac.empty() && frac.back() == '0') frac.pop_back();
        if (frac.empty()) return ip;
    }
    return ip + "." + frac;
}

inline std::string format_frequency(double hz, int precision_digits, char sep, bool trim_trailing_zeros = false) {
    if (precision_digits < 0) precision_digits = 0;

    if (!std::isfinite(hz)) {
        if (std::isnan(hz)) return "nan Hz";
        return (hz < 0 ? "-inf Hz" : "inf Hz");
    }

    const bool neg = (hz < 0);
    double a = std::fabs(hz);

    if (a == 0.0) return "0 Hz";

    struct Unit { double scale; std::string_view name; };
    static constexpr Unit units[] = {
        {1e12, "THz"}, {1e9, "GHz"}, {1e6, "MHz"}, {1e3, "kHz"},
        {1.0,  "Hz"},  {1e-3,"mHz"}, {1e-6,"uHz"},
    };

    // Choose unit so that mantissa is in [1, 1000) when possible
    const Unit* u = &units[4]; // Hz default
    for (const auto& cand : units) {
        double v = a / cand.scale;
        if (v >= 1.0 && v < 1000.0) { u = &cand; break; }
    }
    if (a / units[0].scale >= 1000.0) u = &units[0];
    if (a / units[6].scale < 1.0)     u = &units[6];

    double v = a / u->scale;

    // precision_digits = significant digits
    int decimals = 0;
    if (v > 0.0) {
        int int_digits = static_cast<int>(std::floor(std::log10(v))) + 1; // 1..3 typically
        decimals = precision_digits - int_digits;
        if (decimals < 0) decimals = 0;
    }

//    std::string s = fixed_trim(v, decimals);
  std::string s = fixed(v, decimals, trim_trailing_zeros);

    // Apply grouping to both integer and fractional parts
    if (sep != '\0') {
        auto dot = s.find('.');
        std::string ip = (dot == std::string::npos) ? s : s.substr(0, dot);
        std::string fp = (dot == std::string::npos) ? "" : s.substr(dot + 1);

        ip = group3_int(ip, sep);
        if (!fp.empty()) fp = group3_frac(fp, sep);

        s = ip;
        if (!fp.empty()) { s.push_back('.'); s += fp; }
    }

    if (neg) s.insert(s.begin(), '-');
    s.push_back(' ');
    s.append(u->name.begin(), u->name.end());
    return s;
}

} // namespace freqfmt

/*
Examples (sep = '_'):
  format_frequency(1.23456789e9, 10, '_') -> "1.234_567_89 GHz"
  format_frequency(999.9999999, 10, '_')  -> "999.999_999_9 Hz"
  format_frequency(12.0e6, 3, '_')        -> "12 MHz"           (trimmed)
*/
