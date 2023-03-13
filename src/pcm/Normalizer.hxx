// SPDX-License-Identifier: LGPL-2.1
// Copyright The Music Player Daemon Project
// Based on AudioCompress (c)2007 busybee (http://beesbuzz.biz/

#pragma once

#include <cstdint>

//! Configuration values for the compressor object
struct CompressorConfig {
	int target;
	int maxgain;
	int smooth;
};

struct Compressor;

//! Create a new compressor (use history value of 0 for default)
struct Compressor *
Compressor_new(unsigned int history) noexcept;

//! Delete a compressor
void
Compressor_delete(struct Compressor *) noexcept;

//! Set the history length
void
Compressor_setHistory(struct Compressor *, unsigned int history) noexcept;

//! Get the configuration for a compressor
struct CompressorConfig *
Compressor_getConfig(struct Compressor *) noexcept;

//! Process 16-bit signed data
void
Compressor_Process_int16(struct Compressor *, int16_t *data, unsigned int count) noexcept;

//! TODO: Compressor_Process_int32, Compressor_Process_float, others as needed

//! TODO: functions for getting at the peak/gain/clip history buffers (for monitoring)
