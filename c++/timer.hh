// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Simple timer

#pragma once

#include <chrono>

class Timer {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;

public:
    Timer() : start_(Clock::now()) {}

    void reset() {
        start_ = Clock::now();
    }

    template <typename Duration = std::chrono::milliseconds>
    Duration elapsed() const {
        return std::chrono::duration_cast<Duration>(Clock::now() - start_);
    }
};

// Example:
//    Timer t;
//    std::cout << "Elapsed: "
//              << t.elapsed<std::chrono::milliseconds>().count()
//              << " ms\n";
