// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_AUDIO_FORMAT_SONG_FILTER_HXX
#define MPD_AUDIO_FORMAT_SONG_FILTER_HXX

#include "ISongFilter.hxx"
#include "pcm/AudioFormat.hxx"

class AudioFormatSongFilter final : public ISongFilter {
	AudioFormat value;

public:
	explicit AudioFormatSongFilter(const AudioFormat &_value) noexcept
		:value(_value) {}


	/* virtual methods from ISongFilter */
	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<AudioFormatSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

#endif
