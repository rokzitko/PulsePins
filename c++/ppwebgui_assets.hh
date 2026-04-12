// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

struct WebGuiAssets {
  const char *index_html;
  const char *app_css;
  const char *app_js;
};

extern const char *index_html;
extern const char *app_css;
extern const char *app_js;

WebGuiAssets get_ppwebgui_assets();
