// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_BASE_SONG_FILTER_HXX
#define MPD_BASE_SONG_FILTER_HXX

#include "ISongFilter.hxx"

class BaseSongFilter final : public ISongFilter {
	std::string value;

public:
	BaseSongFilter(const BaseSongFilter &) = default;

	template<typename V>
	explicit BaseSongFilter(V &&_value)
		:value(std::forward<V>(_value)) {}

	const char *GetValue() const noexcept {
		return value.c_str();
	}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<BaseSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

#endif
