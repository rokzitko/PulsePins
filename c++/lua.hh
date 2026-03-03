#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <iostream>
#include <string>

#include "tidbit.hh"
#include "verbosity.hh"
#include "fpga.hh"

static void throw_if_lua_error(lua_State* L, int status) {
  if (status != LUA_OK) {
    const char* msg = lua_tostring(L, -1);
    std::stringstream ss;
    ss << "Lua error: " << (msg ? msg : "(unknown)") << "\n";
    lua_pop(L, 1); // remove error message
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

class lua_processor {
 private:
   const InputParser &input;
   const Verbosity &v;
   FPGA &fpga;
   lua_State *L;

 public:
   lua_processor(const InputParser &_input, const Verbosity &_v, FPGA &_fpga) :
     input(_input), v(_v), fpga(_fpga) {
       L = luaL_newstate(); // create interpreter
       if (!L) throw std::runtime_error("luaL_newstate() failed");
       luaL_openlibs(L);    // standard libs
       // Register C function as global "add"
       lua_pushcfunction(L, l_add);
       lua_setglobal(L, "add");
     }

   ~lua_processor() {
     lua_close(L); // cleanup
   }

   void process_line(const std::string& line) {
     throw_if_lua_error(L, luaL_dostring(L, line.c_str()));
   }

   void test() {
     process_line(R"(
         print("Hello from Lua")
         print("2+3=", add(2,3))
        )");
   }
};
