// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STICKER_HXX
#define MPD_STICKER_HXX

#include <map>
#include <string>

struct Sticker {
	std::map<std::string, std::string, std::less<>> table;
};

#endif
