// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NEIGHBOR_INFO_HXX
#define MPD_NEIGHBOR_INFO_HXX

#include <string>

struct NeighborInfo {
	std::string uri;
	std::string display_name;

	template<typename U, typename DN>
	NeighborInfo(U &&_uri, DN &&_display_name)
		:uri(std::forward<U>(_uri)),
		 display_name(std::forward<DN>(_display_name)) {}
};

#endif
