// SPDX-License-Identifier: LGPL-2.1
// Copyright The Music Player Daemon Project
// Based on AudioCompress (c)2007 busybee (http://beesbuzz.biz/

#pragma once

#include <cstdint>

struct Compressor;

//! Create a new compressor (use history value of 0 for default)
struct Compressor *
Compressor_new(unsigned int history = 400) noexcept;

//! Delete a compressor
void
Compressor_delete(struct Compressor *) noexcept;

//! Process 16-bit signed data
void
Compressor_Process_int16(struct Compressor *, int16_t *data, unsigned int count) noexcept;

//! TODO: Compressor_Process_int32, Compressor_Process_float, others as needed

//! TODO: functions for getting at the peak/gain/clip history buffers (for monitoring)
