// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_VECTOR_HXX
#define MPD_PLAYLIST_VECTOR_HXX

#include "db/PlaylistInfo.hxx"

#include <list>
#include <string_view>

class PlaylistVector : protected std::list<PlaylistInfo> {
protected:
	/**
	 * Caller must lock the #db_mutex.
	 */
	[[gnu::pure]]
	iterator find(std::string_view name) noexcept;

public:
	using std::list<PlaylistInfo>::empty;
	using std::list<PlaylistInfo>::begin;
	using std::list<PlaylistInfo>::end;
	using std::list<PlaylistInfo>::push_back;
	using std::list<PlaylistInfo>::erase;

	/**
	 * Caller must lock the #db_mutex.
	 *
	 * @return true if the vector or one of its items was modified
	 */
	bool UpdateOrInsert(PlaylistInfo &&pi) noexcept;

	/**
	 * Caller must lock the #db_mutex.
	 */
	bool erase(std::string_view name) noexcept;

	[[nodiscard]]
	bool exists(std::string_view name) const noexcept;
};

#endif /* SONGVEC_H */
