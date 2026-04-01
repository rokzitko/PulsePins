// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Threaded zipper that pairs items from two producer queues.

#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <utility>

template <typename A, typename B>
class ZipAggregator {
public:
    using Task = std::function<void(const A&, const B&)>;

    explicit ZipAggregator(Task task, std::size_t max_depth_per_queue = 1024)
        : task_(std::move(task)),
          max_depth_(max_depth_per_queue),
          worker_([this] { run(); }) {}

    ZipAggregator(const ZipAggregator&) = delete;
    ZipAggregator& operator=(const ZipAggregator&) = delete;

    ~ZipAggregator() {
        stop(/*drain=*/true);
    }

    // Blocks if A-queue is full (backpressure).
    // Returns false if stopping/draining state does not accept new data.
    bool submitA(A a) {
        std::unique_lock<std::mutex> lk(m_);
        cv_not_full_a_.wait(lk, [&] { return stopping_ || qa_.size() < max_depth_; });
        if (stopping_) return false;
        qa_.push_back(std::move(a));
        lk.unlock();
        cv_not_empty_.notify_one();
        return true;
    }

    // Blocks if B-queue is full (backpressure).
    bool submitB(B b) {
        std::unique_lock<std::mutex> lk(m_);
        cv_not_full_b_.wait(lk, [&] { return stopping_ || qb_.size() < max_depth_; });
        if (stopping_) return false;
        qb_.push_back(std::move(b));
        lk.unlock();
        cv_not_empty_.notify_one();
        return true;
    }

    // Stop the worker. If drain==true, process all remaining *pairs* before exiting.
    // Unpaired leftovers (extra A without B, or vice versa) remain unprocessed.
    void stop(bool drain) {
        {
            std::lock_guard<std::mutex> lk(m_);
            if (stopping_) return;
            stopping_ = true;
            drain_ = drain;
        }
        cv_not_empty_.notify_all();
        cv_not_full_a_.notify_all();
        cv_not_full_b_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

private:
    void run() {
        std::unique_lock<std::mutex> lk(m_);
        for (;;) {
            cv_not_empty_.wait(lk, [&] {
                // Wake if we can process a pair, or if we're stopping.
                return (qa_.size() > 0 && qb_.size() > 0) || stopping_;
            });

            // If stopping: either exit immediately, or drain remaining pairs.
            if (stopping_) {
                if (!drain_) break;
                if (qa_.empty() || qb_.empty()) break; // no more pairs to drain
            }

            // We have at least one from each: pop one pair.
            A a = std::move(qa_.front()); qa_.pop_front();
            B b = std::move(qb_.front()); qb_.pop_front();

            // Notify producers potentially blocked on "full".
            cv_not_full_a_.notify_one();
            cv_not_full_b_.notify_one();

            // Run task outside the lock.
            lk.unlock();
            task_(a, b);
            lk.lock();
        }
    }

    Task task_;
    const std::size_t max_depth_;

    std::mutex m_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_a_;
    std::condition_variable cv_not_full_b_;

    std::deque<A> qa_;
    std::deque<B> qb_;

    bool stopping_ = false;
    bool drain_ = true;

    std::thread worker_;
};
