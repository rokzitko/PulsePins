#pragma once

#include <thread>
#include <chrono>

void sleep_1us()
{
  std::this_thread::sleep_for(std::chrono::microseconds(1000));
}

void sleep_1ms()
{
  std::this_thread::sleep_for(std::chrono::microseconds(1000));
}

void sleep(const double delay) // time in seconds
{
  std::this_thread::sleep_for(std::chrono::duration<double>(delay));
}
