// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Rok Zitko
//
// Aggregates the main streamer control, FIFO, and DMA interfaces.

#pragma once

const bool output_streamer = false;
const bool output_override = true;

#include "streamer_control.hh"
#include "streamer_dma.hh"
#include "streamer_fifo.hh"
