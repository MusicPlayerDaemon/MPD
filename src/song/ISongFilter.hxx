// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_I_SONG_FILTER_HXX
#define MPD_I_SONG_FILTER_HXX

#include <memory>
#include <string>

struct LightSong;
class ISongFilter;
using ISongFilterPtr = std::unique_ptr<ISongFilter>;

class ISongFilter {
public:
	virtual ~ISongFilter() noexcept = default;

	virtual ISongFilterPtr Clone() const noexcept = 0;

	/**
	 * Convert this object into an "expression".  This is
	 * only useful for debugging.
	 */
	virtual std::string ToExpression() const noexcept = 0;

	[[gnu::pure]]
	virtual bool Match(const LightSong &song) const noexcept = 0;
};

#endif
