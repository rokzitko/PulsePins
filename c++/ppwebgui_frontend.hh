// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <string>

struct WebGuiAssets {
  const char *index_html;
  const char *app_css;
  const char *app_js;
};

struct WebGuiHttpOptions {
  bool veryverbose = false;
};

struct WebGuiServerBinding {
  std::string bind_ip = "0.0.0.0";
  int bind_port = 4242;
};
