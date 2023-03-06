// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DECODER_COMMAND_HXX
#define MPD_DECODER_COMMAND_HXX

#include <cstdint>

enum class DecoderCommand : uint8_t {
	NONE = 0,
	START,
	STOP,
	SEEK
};

#endif
