// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_AND_SONG_FILTER_HXX
#define MPD_AND_SONG_FILTER_HXX

#include "ISongFilter.hxx"

#include <list>

/**
 * Combine multiple #ISongFilter instances with logical "and".
 */
class AndSongFilter final : public ISongFilter {
	std::list<ISongFilterPtr> items;

	friend void OptimizeSongFilter(AndSongFilter &) noexcept;
	friend ISongFilterPtr OptimizeSongFilter(ISongFilterPtr) noexcept;

public:
	const auto &GetItems() const noexcept {
		return items;
	}

	template<typename I>
	void AddItem(I &&_item) {
		items.emplace_back(std::forward<I>(_item));
	}

	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return items.empty();
	}

	/* virtual methods from ISongFilter */
	ISongFilterPtr Clone() const noexcept override;
	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

#endif
