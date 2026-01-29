// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DIRECTORY_SONG_FILTER_HXX
#define MPD_DIRECTORY_SONG_FILTER_HXX

#include "ISongFilter.hxx"

class DirectorySongFilter final : public ISongFilter {
	std::string value;

public:
	DirectorySongFilter(const DirectorySongFilter &) = default;

	template<typename V>
	explicit DirectorySongFilter(V &&_value)
		:value(std::forward<V>(_value)) {}

	const char *GetValue() const noexcept {
		return value.c_str();
	}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<DirectorySongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

#endif
