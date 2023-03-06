// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_ESCAPE_HXX
#define MPD_SONG_ESCAPE_HXX

#include "util/Compiler.h"

#include <string>

gcc_pure
std::string
EscapeFilterString(const std::string &src) noexcept;

#endif
