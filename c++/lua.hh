// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Lua bindings and helpers for PulsePins host-side scripting.

#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <sstream>
#include <thread>
#include <stdexcept>

#include "tidbit.hh"
#include "verbosity.hh"
#include "fpga.hh"

static void throw_if_lua_error(lua_State* L, const int status) {
  if (status != LUA_OK) {
    const char* msg = lua_tostring(L, -1);
    lua_pop(L, 1); // remove error message
    std::stringstream ss;
    ss << "Lua error: " << (msg ? msg : "(unknown)") << "\n";
    throw std::runtime_error(ss.str());
  }
}

// Example: a C function callable from Lua: add(2,3) -> 5
static int l_add(lua_State* L) {
    // Lua stack: args at 1..n
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a + b); // return value
    return 1; // number of return values
}

using Fn = std::function<int(lua_State*)>;

struct lua_state_deleter {
  void operator()(lua_State *state) const noexcept {
    if (state)
      lua_close(state);
  }
};

// trampoline, called from lua
static int fn_trampoline(lua_State* L) {
  // upvalue 1: full userdata storing std::shared_ptr<Fn>
  auto* p = static_cast<std::shared_ptr<Fn>*>(lua_touserdata(L, lua_upvalueindex(1)));
  return (**p)(L); // invoke C++ callable, pass the lua_State, return the number of values left of stack
}

// garbage collection support, called to collect userdata
static int fn_gc(lua_State* L) {
  auto* p = static_cast<std::shared_ptr<Fn>*>(lua_touserdata(L, 1));
  p->~shared_ptr<Fn>(); // call destructor manually
  return 0;
}

// helper to ensure metatable CppFnUD exists
// Any userdata tagged with CppFnUD will run fn_gc when collected.
static void ensure_fn_ud_metatable(lua_State* L) {
  if (luaL_newmetatable(L, "CppFnUD")) {          // stack: mt
    lua_pushcfunction(L, fn_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);                                  // pop mt
}

// Generic wrapper. Accepts any callable f (e.g. lambda with closure, functor, std::function)
template <class F>
void lua_push_function_object(lua_State* L, F&& f) {
  ensure_fn_ud_metatable(L);
  // allocate userdata to hold shared_ptr<Fn>; lua_newuserdatauv is Lua 5.4 API
  void* ud = lua_newuserdatauv(L, sizeof(std::shared_ptr<Fn>), 0); // stack: ud = raw userdata memory
  new (ud) std::shared_ptr<Fn>(std::make_shared<Fn>(std::forward<F>(f)));
  luaL_getmetatable(L, "CppFnUD");                 // stack: ud, mt
  lua_setmetatable(L, -2);                         // stack: ud
  // create closure: trampoline with 1 upvalue (the userdata)
  lua_pushcclosure(L, fn_trampoline, 1);           // stack: closure
}

class lua_processor {
private:
  const InputParser &input;
  const Verbosity &v;
  FPGA &fpga;
  std::unique_ptr<lua_State, lua_state_deleter> L;

public:
  lua_processor(const InputParser &_input, const Verbosity &_v, FPGA &_fpga) :
    input(_input), v(_v), fpga(_fpga), L(luaL_newstate()) {
      if (!L) throw std::runtime_error("luaL_newstate() failed");
      lua_State *state = L.get();
      luaL_openlibs(state); // standard libs
      // Register C function as global "add"
      lua_pushcfunction(state, l_add);
      lua_setglobal(state, "add");
      lua_push_function_object(state, [capture = 42](lua_State* L) -> int {
        lua_pushinteger(L, capture);
        return 1; // number of return values
      });
      lua_setglobal(state, "get_capture");
      lua_push_function_object(state, [&](lua_State *L) -> int {
        int p = luaL_checknumber(L, 1);
        fpga.trig_int.trig(p);
        return 0;
      });
      lua_setglobal(state, "trig");
    }

  lua_processor(const lua_processor&) = delete;
  lua_processor& operator=(const lua_processor&) = delete;
  lua_processor(lua_processor&&) = delete;
  lua_processor& operator=(lua_processor&&) = delete;
  ~lua_processor() = default;

  void process_line(const std::string& line) {
    throw_if_lua_error(L.get(), luaL_dostring(L.get(), line.c_str()));
  }

  void test() {
    process_line(R"(
        print("Hello from Lua")
        -- print("2+3=", add(2,3))
        -- print(get_capture())
        )");
  }
};
