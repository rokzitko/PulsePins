#pragma once

#include <thread>
#include <chrono>

inline void sleep_1us()
{
  std::this_thread::sleep_for(std::chrono::microseconds(1));
}

inline void sleep_1ms()
{
  std::this_thread::sleep_for(std::chrono::microseconds(1000));
}

inline void sleep(const double delay) // time in seconds
{
  std::this_thread::sleep_for(std::chrono::duration<double>(delay));
}
