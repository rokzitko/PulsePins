// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Command handler declarations for the `pptool` executable family.

#pragma once

#include "fpga.hh"
#include "parser.hh"
#include "verbosity.hh"

int pptool(FPGA &fpga, const InputParser &input, const Verbosity &v);
int pptest(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppmstest(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppdmatest(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppfg(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppdelay(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppreset(FPGA &fpga, const InputParser &input, const Verbosity &v);
int pptrig(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppqout(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppaux(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppcounter(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppread(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppts(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppgpsdo(FPGA &fpga, const InputParser &input, const Verbosity &v);
int pptemp(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppfreq(FPGA &fpga, const InputParser &input, const Verbosity &v);
int ppvcd(FPGA &fpga, const InputParser &input, const Verbosity &v);
int pphelloworld(FPGA &fpga, const InputParser &input, const Verbosity &v);
