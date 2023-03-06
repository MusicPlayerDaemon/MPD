// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LIGHT_DIRECTORY_HXX
#define MPD_LIGHT_DIRECTORY_HXX

#include <chrono>
#include <string>

struct Tag;

/**
 * A reference to a directory.  Unlike the #Directory class, this one
 * consists only of pointers.  It is supposed to be as light as
 * possible while still providing all the information MPD has about a
 * directory.  This class does not manage any memory, and the pointers
 * become invalid quickly.  Only to be used to pass around during
 * well-defined situations.
 */
struct LightDirectory {
	const char *uri;

	std::chrono::system_clock::time_point mtime;

	constexpr LightDirectory(const char *_uri,
				 std::chrono::system_clock::time_point _mtime)
		:uri(_uri), mtime(_mtime) {}

	static constexpr LightDirectory Root() noexcept {
		return LightDirectory("", std::chrono::system_clock::time_point::min());
	}

	bool IsRoot() const noexcept {
		return *uri == 0;
	}

	[[gnu::pure]]
	const char *GetPath() const noexcept {
		return uri;
	}
};

#endif
