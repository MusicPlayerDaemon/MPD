// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
