// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Miscellaneous general functions

#pragma once

#include <cassert>
#include <iostream>
#include <string>
#include <filesystem>

#include "ppversion.hh"

#include "tidbit.hh"
#include "socal/alt_fpgamgr.h"
#include "hps_0.h"

#ifndef HPS_REGS_OFST
#define HPS_REGS_OFST  0xFF700000
#endif
#ifndef HPS_REGS_RANGE
#define HPS_REGS_RANGE 0x00010000
#endif

#define ALT_SYSMGR_BASE          0xFFD08000
#define ALT_SYSMGR_RANGE         0x4000
#define HPS_TO_FPGA_GP_OUT_OFST  0x400
#define HPS_TO_FPGA_GP_IN_OFST   0x410  // 0xFFD08410 absolute

#define ALT_FPGAMGR_BASE         0xFF706000
#define ALT_FPGAMGR_RANGE        0x1000

static_assert(ALT_FPGAMGR_BASE == ALT_FPGAMGR_OFST);

#include "sysid.hh"

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

inline void about(std::string progname, std::string author = "Rok Zitko, rok.zitko@ijs.si")
{
  std::cout << progname << ", " << author << std::endl;
  std::cout << "Version " << VERSION << ", commit " << TOSTRING(GIT_HASH) << ", compiled on " << __DATE__ << " " << __TIME__ << std::endl;
}

// Returns the name of the executable called (symlink)
inline auto get_program_name([[maybe_unused]] int argc, char *argv[])
{
  return std::filesystem::path(argv[0]).filename().string();
}

inline void check_version(const int version, const bool verbose = false)
{
  if (SYSID_QSYS_0_ID != tidbit)
    throw std::runtime_error("Host build expects a different FPGA design ID.");
  if (SYSID_QSYS_1_ID != version)
    throw std::runtime_error("Host build expects a different FPGA version ID.");

  mm dev_lw(LWHPSFPGA_OFST, LWH2F_RANGE, "lw");
  mm dev_h2f(HPSFPGA_OFST, H2F_RANGE, "h2f");
  sysid id(dev_lw,   SYSID_BASE,        SYSID_ID,        verbose, "id");
  sysid id0(dev_lw,  SYSID_QSYS_0_BASE, SYSID_QSYS_0_ID, verbose, "id0");
  sysid id1(dev_lw,  SYSID_QSYS_1_BASE, SYSID_QSYS_1_ID, verbose, "id1");
  sysid id2(dev_h2f, SYSID_H2F_BASE,    SYSID_H2F_ID,    verbose, "id2");
  // These tests also ensure that we can communicate on both lw and h2f buses.

  std::cout << "Bitstream timestamp: " << id.get_timestamp_string() << std::endl;
}
