// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Rate-limit function calls (useful for periodic reporting)

#pragma once

#include <chrono>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;

class Throttler {
    Clock::time_point last_call;
    std::chrono::seconds min_interval;

public:
    Throttler(int seconds)
        : last_call(Clock::now() - std::chrono::seconds(seconds)),
          min_interval(seconds) {}

    template <typename Func>
    bool try_call(Func f) {
        auto now = Clock::now();
        if (now - last_call >= min_interval) {
            f();
            last_call = now;
            return true;  // function executed
        }
        return false;     // skipped
    }
};

/* Example
int main() {
    Throttler t(2); // allow call only once every 2 seconds

    for (int i = 0; i < 10; ++i) {
        if (t.try_call([]{ std::cout << "Executed\n"; })) {
            std::cout << "Call accepted\n";
        } else {
            std::cout << "Call skipped\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
*/
