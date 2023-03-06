// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ReplayGainParser.hxx"
#include "VorbisComment.hxx"
#include "ReplayGainInfo.hxx"
#include "util/ASCII.hxx"
#include "util/NumberParser.hxx"

#include <cassert>

template<typename T>
static bool
ParseReplayGainTagTemplate(ReplayGainInfo &info, const T t) noexcept
{
	const char *value;

	if ((value = t["replaygain_track_gain"]) != nullptr) {
		info.track.gain = ParseFloat(value);
		return true;
	} else if ((value = t["replaygain_album_gain"]) != nullptr) {
		info.album.gain = ParseFloat(value);
		return true;
	} else if ((value = t["replaygain_track_peak"]) != nullptr) {
		info.track.peak = ParseFloat(value);
		return true;
	} else if ((value = t["replaygain_album_peak"]) != nullptr) {
		info.album.peak = ParseFloat(value);
		return true;
	} else
		return false;

}

bool
ParseReplayGainTag(ReplayGainInfo &info,
		   const char *name, const char *value) noexcept
{
	assert(name != nullptr);
	assert(value != nullptr);

	struct NameValue {
		const char *name;
		const char *value;

		[[gnu::pure]]
		const char *operator[](const char *n) const noexcept {
			return StringEqualsCaseASCII(name, n)
				? value
				: nullptr;
		}
	};

	return ParseReplayGainTagTemplate(info, NameValue{name, value});
}

bool
ParseReplayGainVorbis(ReplayGainInfo &info, std::string_view entry) noexcept
{
	struct VorbisCommentEntry {
		std::string_view entry;

		[[gnu::pure]]
		const char *operator[](std::string_view n) const noexcept {
			return GetVorbisCommentValue(entry, n).data();
		}
	};

	return ParseReplayGainTagTemplate(info, VorbisCommentEntry{entry});
}
