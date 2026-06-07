// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#pragma once

#include <cstdint>
#include <limits>

#include <nanobind/nanobind.h>

#include "address_map.hh"

namespace nb = nanobind;

// The Python API still accepts raw base addresses; adapt them at the binding boundary.
inline address_map::H2fRegion pp_bind_h2f_region(const std::uintptr_t base) noexcept {
  return {base, std::numeric_limits<std::uintptr_t>::max()};
}

inline address_map::LwRegion pp_bind_lw_region(const std::uintptr_t base) noexcept {
  return {base, std::numeric_limits<std::uintptr_t>::max()};
}

void bind_misc(nb::module_ &m);
void bind_sequence(nb::module_ &m);
void bind_hw_base(nb::module_ &m);
void bind_streaming(nb::module_ &m);
void bind_signal_paths(nb::module_ &m);
void bind_counter_bindings(nb::module_ &m);
