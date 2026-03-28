// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Configure verbosity level of the tool

#pragma once

struct Verbosity {
  bool verbose = true;        // enable by default; use -quiet to disable
  bool veryverbose = false;   // use -veryverbose to enable
  bool verbosecheck = false;  // use -verbosecheck to enable
};

inline Verbosity set_verbosity(InputParser &input)
{
  Verbosity v;
  if (input.exists("-quiet"))
    v.verbose = false;
  if (input.exists("-veryverbose"))
    v.veryverbose = true;
  if (input.exists("-verbosecheck"))
    v.verbosecheck = true;
  return v;
}
