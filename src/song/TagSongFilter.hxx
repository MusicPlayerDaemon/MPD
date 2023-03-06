// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_SONG_FILTER_HXX
#define MPD_TAG_SONG_FILTER_HXX

#include "ISongFilter.hxx"
#include "StringFilter.hxx"

#include <cstdint>

enum TagType : uint8_t;
struct Tag;
struct LightSong;

class TagSongFilter final : public ISongFilter {
	TagType type;

	StringFilter filter;

public:
	TagSongFilter(TagType _type, StringFilter &&_filter) noexcept
		:type(_type), filter(std::move(_filter)) {}

	TagType GetTagType() const {
		return type;
	}

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
		return std::make_unique<TagSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;

private:
	bool Match(const Tag &tag) const noexcept;
};

#endif
