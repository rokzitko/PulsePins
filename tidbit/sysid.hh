// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// System identification

#pragma once

#include "memory.hh"

std::string format_sysid_timestamp(uint32_t raw_ts) {
  // Convert to time_t (safe since sysid uses epoch seconds)
  std::time_t t = static_cast<std::time_t>(raw_ts);
  std::tm tm{};
  localtime_r(&t, &tm);
  std::ostringstream oss;
  // __DATE__ equivalent: "Sep 29 2025"
  oss << std::put_time(&tm, "%b %e %Y") << " ";
  // __TIME__ equivalent: "22:11:03"
  oss << std::put_time(&tm, "%H:%M:%S");
  return oss.str();
}

class sysid
{
 private:
   loc lid, // id
       lts; // timestamp
   uint32_t id, ts;
 public:
   sysid(mm &dev, const std::uintptr_t base) :
     lid(dev.get_loc(base)),
     lts(dev.get_loc(base, 4))
   {
     id = lid.read();
     ts = lts.read();
   }
   sysid(mm &dev, const std::uintptr_t base, const uint32_t ref) : sysid(dev, base) {
     if (id != ref) throw std::runtime_error("sysid does not match");
   }
   sysid(mm &dev, const std::uintptr_t base, const uint32_t ref, const bool verbose, std::ostream &f = std::cout) : sysid(dev, base) {
     if (verbose) {
       f << "sysid=0x" << std::hex <<  id << " " << std::dec << "(" <<  id << ")" << std::endl;
       if (id != ref) // report reference value only if there is no match, to avoid cluttering output
         f << "  ref=0x" << std::hex << ref << " " << std::dec << "(" << ref << ")" << std::endl;
       f << "timestamp=0x" << std::hex << ts << " " << format_sysid_timestamp(ts) << std::endl;
     }
     if (id != ref) throw std::runtime_error("sysid does not match");
   }
   auto get_id() {
     return id;
   }
   auto get_timestamp() {
     return ts;
   }
   auto get_timestamp_string() {
     return format_sysid_timestamp(ts);
   }
};
