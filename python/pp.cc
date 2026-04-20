// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include "pp_bind.hh"

NB_MODULE(pp, m) {
  m.doc() = "PulsePins Python bindings";
  m.attr("the_answer") = 42;

  bind_misc(m);
  bind_sequence(m);
  bind_hw_base(m);
  bind_streaming(m);
  bind_signal_paths(m);
  bind_counter_bindings(m);
}
