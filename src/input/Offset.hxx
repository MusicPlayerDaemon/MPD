// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OFFSET_HXX
#define MPD_OFFSET_HXX

#include <cstdint>

/**
 * A type for absolute offsets in a file.
 */
typedef uint64_t offset_type;

/**
 * To format an offset_type with printf().  To use this, include
 * <cinttypes>.
 */
#define PRIoffset PRIu64

#endif
