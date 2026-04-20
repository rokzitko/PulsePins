// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#pragma once

#include <nanobind/nanobind.h>

namespace nb = nanobind;

void bind_misc(nb::module_ &m);
void bind_sequence(nb::module_ &m);
void bind_hw_base(nb::module_ &m);
void bind_streaming(nb::module_ &m);
void bind_signal_paths(nb::module_ &m);
void bind_counter_bindings(nb::module_ &m);
