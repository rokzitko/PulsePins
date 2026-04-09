// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// System identification

#pragma once

#include "memory.hh"

inline std::string format_sysid_timestamp(uint32_t raw_ts) {
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
   sysid(mm &dev,
         const std::uintptr_t base,
         std::string name = "sysid"s) :
     lid(dev.get_loc(base), name + "/id"),
     lts(dev.get_loc(base, 4), name + "/ts")
   {
     id = lid.read();
     ts = lts.read();
   }
   sysid(mm &dev,
         const std::uintptr_t base,
         const uint32_t ref) :
     sysid(dev, base)
     {
       if (id != ref) throw std::runtime_error("sysid does not match");
     }
   sysid(mm &dev,
         const std::uintptr_t base,
         const uint32_t ref,
         const bool verbose,
         std::string name = "sysid"s,
         std::ostream &f = std::cout) :
     sysid(dev, base, name) {
       if (verbose) {
       f << "sysid=0x" << std::hex <<  id << " " << std::dec << "(" <<  id << ")" << std::endl;
       if (id != ref) // report reference value only if there is no match, to avoid cluttering output
         f << "  ref=0x" << std::hex << ref << " " << std::dec << "(" << ref << ")" << std::endl;
       f << "timestamp=0x" << std::hex << ts << " " << format_sysid_timestamp(ts) << std::endl;
     }
     if (id != ref) throw std::runtime_error("sysid does not match");
   }
   sysid(mm &dev,
         const std::uintptr_t base,
         const uint32_t ref,
         const bool verbose,
         std::ostream &f = std::cout) :
     sysid(dev, base, ref, verbose, "sysid", f) {}
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

inline void check_ID(const int tidbit, bool verbose = true, std::ostream &s = std::cout)
{
  mm dev_lw(LWHPSFPGA_OFST, LWH2F_RANGE);
  mm dev_h2f(HPSFPGA_OFST, H2F_RANGE);
  assert(SYSID_QSYS_0_ID == tidbit); // check for consistency
  sysid id(dev_lw, SYSID_BASE, SYSID_ID, verbose, s);                      // lw
  sysid id_tidbit(dev_lw, SYSID_QSYS_0_BASE, SYSID_QSYS_0_ID, verbose, s); // lw
  sysid id2(dev_h2f, SYSID_H2F_BASE, SYSID_H2F_ID, verbose, s);            // h2f
}
