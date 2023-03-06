// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_URI_SONG_FILTER_HXX
#define MPD_URI_SONG_FILTER_HXX

#include "ISongFilter.hxx"
#include "StringFilter.hxx"

class UriSongFilter final : public ISongFilter {
	StringFilter filter;

public:
	UriSongFilter(StringFilter &&_filter) noexcept
		:filter(std::move(_filter)) {}

	const auto &GetValue() const noexcept {
		return filter.GetValue();
	}

	bool GetFoldCase() const {
		return filter.GetFoldCase();
	}

	bool IsNegated() const noexcept {
		return filter.IsNegated();
	}

	void ToggleNegated() noexcept {
		filter.ToggleNegated();
	}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<UriSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

#endif
