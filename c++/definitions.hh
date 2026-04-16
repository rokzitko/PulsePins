// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Shared constants for streamer limits and tool return codes.

#pragma once

#include "config.h"

// Max. nr. of elements that can be queued in (assuming no replay elements).
constexpr auto max_size =
  (FIFO_1_IN_FIFO_DEPTH*(FIFO_1_IN_AVALONMM_AVALONST_DATA_WIDTH/8))/BYTES_TOTAL // Avalon ST FIFO
  + SIZE_FIFO_IN1   // FIFO 1 in input_fifo
  + SIZE_FIFO_IN2   // FIFO 2 in input_fifo
  - 2*almost_shift  // because we are using almostfull for stalling
  + 1;

inline constexpr int RC_OK = 0;
inline constexpr int RC_EXCEPTION = 1;
inline constexpr int RC_INVALID_ARG = 2;
inline constexpr int RC_ERROR_CHECK = 4;
inline constexpr int RC_ERROR_CRC_MISMATCH = 8;
inline constexpr int RC_ERROR_BUFFER_ERROR = 16;
inline constexpr int RC_ERROR_OVERFLOW = 32;
inline constexpr int RC_TIMEOUT = 64;
