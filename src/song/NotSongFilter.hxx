// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NOT_SONG_FILTER_HXX
#define MPD_NOT_SONG_FILTER_HXX

#include "ISongFilter.hxx"

/**
 * Negate an #ISongFilter.
 */
class NotSongFilter final : public ISongFilter {
	ISongFilterPtr child;

	friend ISongFilterPtr OptimizeSongFilter(ISongFilterPtr) noexcept;

public:
	template<typename C>
	explicit NotSongFilter(C &&_child) noexcept
		:child(std::forward<C>(_child)) {}

	/* virtual methods from ISongFilter */
	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<NotSongFilter>(child->Clone());
	}

	std::string ToExpression() const noexcept override {
		return "(!" + child->ToExpression() + ")";
	}

	bool Match(const LightSong &song) const noexcept override {
		return !child->Match(song);
	}
};

#endif
