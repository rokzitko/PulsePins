// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <string>
#include <string_view>

#include "ppwebgui_types.hh"

std::string json_escape(std::string_view input);
std::string status_to_json(const StatusSnapshot &status);
std::string operation_json(const std::string &message, const StatusSnapshot &status, int rc = RC_OK);
