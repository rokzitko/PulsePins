// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Miscelanous general functions

#pragma once

#include <iostream>
#include <string>
#include <filesystem>

#include "sysid.hh"

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

void about(std::string progname, std::string author = "Rok Zitko, rok.zitko@ijs.si")
{
  std::cout << progname << ", " << author << std::endl;
  std::cout << "Version " << VERSION << ", commit " << TOSTRING(GIT_HASH) << ", compiled on " << __DATE__ << " " << __TIME__ << std::endl;
}

// Returns the name of the executable called (symlink)
auto get_program_name(int argc, char *argv[])
{
  return std::filesystem::path(argv[0]).filename().string();
}

inline void check_version(const int version, const bool verbose = false)
{
  assert(SYSID_QSYS_0_ID == tidbit);  // expected FPGA design?
  assert(SYSID_QSYS_1_ID == version); // expected version of the design?

  mm dev_lw(LWHPSFPGA_OFST, LWH2F_RANGE);
  mm dev_h2f(HPSFPGA_OFST, H2F_RANGE);
  sysid id(dev_lw,   SYSID_BASE,        SYSID_ID,        verbose);
  sysid id0(dev_lw,  SYSID_QSYS_0_BASE, SYSID_QSYS_0_ID, verbose);
  sysid id1(dev_lw,  SYSID_QSYS_1_BASE, SYSID_QSYS_1_ID, verbose);
  sysid id2(dev_h2f, SYSID_H2F_BASE,    SYSID_H2F_ID,    verbose);
  // These tests also ensure that we can communicate on both lw and h2f buses.

  std::cout << "Bitstream timestamp: " << id.get_timestamp_string() << std::endl;
}
