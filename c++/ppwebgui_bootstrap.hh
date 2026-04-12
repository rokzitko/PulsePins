// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <string>

void install_ppwebgui_fatal_signal_handlers();
void print_ppwebgui_startup_urls(const std::string &bind_ip, int actual_port);
