// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

namespace httplib {
class Server;
}

class WebGuiService;

struct WebGuiHttpOptions {
  bool veryverbose = false;
};

void register_ppwebgui_routes(httplib::Server &server,
                              WebGuiService &service,
                              WebGuiHttpOptions options,
                              const char *index_html,
                              const char *app_css,
                              const char *app_js);
