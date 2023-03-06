// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_STRING_HXX
#define MPD_TAG_STRING_HXX

#include <string_view>

template<typename T> class AllocatedArray;

AllocatedArray<char>
FixTagString(std::string_view p) noexcept;

#endif
