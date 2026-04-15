// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Simple command-line parameter parser

#pragma once

#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Simple command line parser
// based on https://stackoverflow.com/questions/865668/parsing-command-line-arguments-in-c
class InputParser {
 public:
   InputParser(int argc, char *argv[]) {
     for (int i = 1; i < argc; ++i)
       this->tokens.push_back(std::string(argv[i]));
   }
   InputParser(std::vector<std::string> t) : tokens(t) {}
   std::string get(const std::string &option) const {
     auto itr = std::find(this->tokens.begin(), this->tokens.end(), option);
     if (itr != this->tokens.end() && ++itr != this->tokens.end())
       return *itr;
     return "";
   }
   bool exists(const std::string &option) const {
     return std::find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
   }
   std::string get_string(const std::string &option, const std::string def) const {
     return exists(option) ? require_arg(option) : def;
   }
   double get_double(const std::string &option, const double def) const {
     return exists(option) ? std::stod(require_arg(option)) : def;
   }
   uint32_t get_uint32(const std::string &option, const uint32_t def) const {
     return exists(option) ? std::stoul(require_arg(option)) : def;
   }
   uint64_t get_uint64(const std::string &option, const uint64_t def) const {
     return exists(option) ? std::stoull(require_arg(option)) : def;
   }
   void add(const std::string s) {
     tokens.push_back(s);
   }
   void add_with_arg(const std::string s1, const std::string s2) {
     tokens.push_back(s1);
     tokens.push_back(s2);
   }
   std::optional<std::string> first_arg() const {
     if (tokens.size() > 0)
       return tokens.front();
     else
       return std::nullopt;
   }
   std::optional<int> first_arg_int() const {
     if (tokens.size() > 0) {
       auto s = tokens.front();
       if (s.empty())
         return std::nullopt;
       const auto c = static_cast<unsigned char>(s.front());
       if (std::isdigit(c))
         return std::stoi(s);
     }
     return std::nullopt;
   }
 private:
   std::string require_arg(const std::string &option) const {
     auto itr = std::find(this->tokens.begin(), this->tokens.end(), option);
     if (itr == this->tokens.end())
       throw std::runtime_error("Missing option: " + option);
     ++itr;
     if (itr == this->tokens.end())
       throw std::runtime_error("Option " + option + " requires an argument");
     return *itr;
   }

   std::vector<std::string> tokens;
};
