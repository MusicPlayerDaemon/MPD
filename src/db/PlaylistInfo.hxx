/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_PLAYLIST_INFO_HXX
#define MPD_PLAYLIST_INFO_HXX

#include <string>
#include <string_view>
#include <chrono>

/**
 * A directory entry pointing to a playlist file.
 */
struct PlaylistInfo {
	/**
	 * The UTF-8 encoded name of the playlist file.
	 */
	std::string name;

	/**
	 * The time stamp of the last file modification.  A negative
	 * value means that this is unknown/unavailable.
	 */
	std::chrono::system_clock::time_point mtime =
		std::chrono::system_clock::time_point::min();

	class CompareName {
		const std::string_view name;

	public:
		constexpr CompareName(std::string_view _name) noexcept
			:name(_name) {}

		[[gnu::pure]]
		bool operator()(const PlaylistInfo &pi) const noexcept {
			return pi.name == name;
		}
	};

	PlaylistInfo() = default;

	template<typename N>
	explicit PlaylistInfo(N &&_name,
			      std::chrono::system_clock::time_point _mtime=std::chrono::system_clock::time_point::min())
		:name(std::forward<N>(_name)), mtime(_mtime) {}

	PlaylistInfo(const PlaylistInfo &other) = delete;
	PlaylistInfo(PlaylistInfo &&) = default;
};

#endif
