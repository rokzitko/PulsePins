// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Linux realtime scheduler control

#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#if defined(__linux__)
#include <sched.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#endif

class SchedulerError : public std::runtime_error {
public:
    explicit SchedulerError(const std::string& what_arg)
        : std::runtime_error(what_arg) {}
};

#if defined(__linux__)
class RealtimeScheduler {
public:
    RealtimeScheduler(int policy = SCHED_FIFO, int priority = -1) {
        // Save current scheduler and parameters
        orig_policy = sched_getscheduler(0);
        if (orig_policy == -1) {
            throw SchedulerError("sched_getscheduler failed: " +
                                 std::string(std::strerror(errno)));
        }

        if (sched_getparam(0, &orig_param) == -1) {
            throw SchedulerError("sched_getparam failed: " +
                                 std::string(std::strerror(errno)));
        }

        // Resolve priority
        sched_param param{};
        if (priority < 0) {
            int max_prio = sched_get_priority_max(policy);
            if (max_prio == -1) {
                throw SchedulerError("sched_get_priority_max failed: " +
                                     std::string(std::strerror(errno)));
            }
            param.sched_priority = max_prio;
        } else {
            int min_prio = sched_get_priority_min(policy);
            int max_prio = sched_get_priority_max(policy);
            if (min_prio == -1 || max_prio == -1) {
                throw SchedulerError("sched_get_priority_[min|max] failed: " +
                                     std::string(std::strerror(errno)));
            }
            if (priority < min_prio || priority > max_prio) {
                throw SchedulerError("Priority " + std::to_string(priority) +
                                     " is out of range (" +
                                     std::to_string(min_prio) + "-" +
                                     std::to_string(max_prio) + ")");
            }
            param.sched_priority = priority;
        }

        if (sched_setscheduler(0, policy, &param) == -1) {
            throw SchedulerError("sched_setscheduler failed: " +
                                 std::string(std::strerror(errno)));
        }

        new_policy = policy;
        new_param = param;
    }

    ~RealtimeScheduler() {
        if (orig_policy != -1) {
            if (sched_setscheduler(0, orig_policy, &orig_param) == -1) {
                std::cerr << "Warning: failed to restore scheduler: "
                          << std::strerror(errno) << '\n';
            }
        }
    }

    RealtimeScheduler(const RealtimeScheduler&) = delete;
    RealtimeScheduler& operator=(const RealtimeScheduler&) = delete;

    RealtimeScheduler(RealtimeScheduler&& other) noexcept {
        *this = std::move(other);
    }

    RealtimeScheduler& operator=(RealtimeScheduler&& other) noexcept {
        if (this != &other) {
            orig_policy = other.orig_policy;
            orig_param = other.orig_param;
            new_policy = other.new_policy;
            new_param = other.new_param;
            other.orig_policy = -1;
        }
        return *this;
    }

    static std::string report() {
        int policy = sched_getscheduler(0);
        if (policy == -1) {
            throw SchedulerError("sched_getscheduler failed: " +
                                 std::string(std::strerror(errno)));
        }

        sched_param param{};
        if (sched_getparam(0, &param) == -1) {
            throw SchedulerError("sched_getparam failed: " +
                                 std::string(std::strerror(errno)));
        }

        std::string pol_name;
        switch (policy) {
            case SCHED_OTHER: pol_name = "SCHED_OTHER"; break;
            case SCHED_BATCH: pol_name = "SCHED_BATCH"; break;
            case SCHED_IDLE:  pol_name = "SCHED_IDLE";  break;
            case SCHED_FIFO:  pol_name = "SCHED_FIFO";  break;
            case SCHED_RR:    pol_name = "SCHED_RR";    break;
#ifdef SCHED_DEADLINE
            case SCHED_DEADLINE: pol_name = "SCHED_DEADLINE"; break;
#endif
            default: pol_name = "UNKNOWN"; break;
        }

        return pol_name + ", priority " + std::to_string(param.sched_priority);
    }

private:
    int orig_policy{};
    sched_param orig_param{};
    int new_policy{};
    sched_param new_param{};
};
#else
class RealtimeScheduler {
public:
    RealtimeScheduler(int policy = 0, int priority = -1) {
        (void)policy;
        (void)priority;
    }

    ~RealtimeScheduler() = default;

    RealtimeScheduler(const RealtimeScheduler&) = delete;
    RealtimeScheduler& operator=(const RealtimeScheduler&) = delete;

    RealtimeScheduler(RealtimeScheduler&&) noexcept = default;
    RealtimeScheduler& operator=(RealtimeScheduler&&) noexcept = default;

    static std::string report() {
        return "realtime scheduling unsupported on this platform";
    }
};
#endif
