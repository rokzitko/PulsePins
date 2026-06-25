// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// File-format selection helpers for sequence playback commands.

#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "parser.hh"
#include "vcd_parser.hh"

enum class SequenceFileFormat {
  vcd,
  text,
  binary
};

inline SequenceFileFormat parse_sequence_file_format(const std::string &s)
{
  if (s == "vcd")
    return SequenceFileFormat::vcd;
  if (s == "text")
    return SequenceFileFormat::text;
  if (s == "binary")
    return SequenceFileFormat::binary;
  throw std::runtime_error("Unknown sequence file format: " + s);
}

inline SequenceFileFormat infer_sequence_file_format_from_filename(const std::string &filename)
{
  if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".vcd")
    return SequenceFileFormat::vcd;
  if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".seq")
    return SequenceFileFormat::text;
  if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".txt")
    return SequenceFileFormat::text;
  if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".bin")
    return SequenceFileFormat::binary;
  if (filename.size() >= 6 && filename.substr(filename.size() - 6) == ".ppbin")
    return SequenceFileFormat::binary;
  throw std::runtime_error("Cannot infer sequence file format from extension; use -format vcd|text|binary");
}

inline SequenceFileFormat resolve_sequence_file_format(const InputParser &input,
                                                      const std::string &filename,
                                                      std::optional<SequenceFileFormat> forced_default = std::nullopt)
{
  if (input.exists("-format"))
    return parse_sequence_file_format(input.get_string("-format", ""));
  if (forced_default.has_value())
    return *forced_default;
  return infer_sequence_file_format_from_filename(filename);
}

inline uint32_t parse_vcd_scale_factor(const InputParser &input)
{
  const auto scale_factor = input.get_uint32("-scale", default_vcd_scale_factor);
  if (scale_factor == 0)
    throw std::runtime_error("Option -scale must be greater than zero");
  return scale_factor;
}

inline void validate_sequence_file_options(const InputParser &input,
                                          const SequenceFileFormat format)
{
  if (format != SequenceFileFormat::vcd) {
    if (input.exists("-target"))
      throw std::runtime_error("Option -target is only valid for VCD input");
    if (input.exists("-scale"))
      throw std::runtime_error("Option -scale is only valid for VCD input");
  }
  if (format == SequenceFileFormat::vcd)
    (void)parse_vcd_scale_factor(input);
}
