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
