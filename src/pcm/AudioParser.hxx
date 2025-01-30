// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * Parser functions for audio related objects.
 */

#pragma once

#include <string_view>

struct AudioFormat;

/**
 * Parses a string in the form "SAMPLE_RATE:BITS:CHANNELS" into an
 * #AudioFormat.
 *
 * Throws #std::runtime_error on error.
 *
 * @param src the input string
 * @param mask if true, then "*" is allowed for any number of items
 */
AudioFormat
ParseAudioFormat(std::string_view src, bool mask);
