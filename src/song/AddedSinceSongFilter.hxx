// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ADDED_SINCE_SONG_FILTER_HXX
#define MPD_ADDED_SINCE_SONG_FILTER_HXX

#include "ISongFilter.hxx"

#include <chrono>

class AddedSinceSongFilter final : public ISongFilter {
	std::chrono::system_clock::time_point value;

public:
	explicit AddedSinceSongFilter(std::chrono::system_clock::time_point _value) noexcept
		:value(_value) {}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<AddedSinceSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

#endif
