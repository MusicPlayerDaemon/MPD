/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ReplayGainParser.hxx"
#include "VorbisComment.hxx"
#include "ReplayGainInfo.hxx"
#include "util/ASCII.hxx"
#include "util/NumberParser.hxx"
#include "util/StringView.hxx"

#include <cassert>

template<typename T>
static bool
ParseReplayGainTagTemplate(ReplayGainInfo &info, const T t)
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
ParseReplayGainTag(ReplayGainInfo &info, const char *name, const char *value)
{
	assert(name != nullptr);
	assert(value != nullptr);

	struct NameValue {
		const char *name;
		const char *value;

		gcc_pure
		const char *operator[](const char *n) const noexcept {
			return StringEqualsCaseASCII(name, n)
				? value
				: nullptr;
		}
	};

	return ParseReplayGainTagTemplate(info, NameValue{name, value});
}

bool
ParseReplayGainVorbis(ReplayGainInfo &info, StringView entry)
{
	struct VorbisCommentEntry {
		StringView entry;

		gcc_pure
		const char *operator[](StringView n) const noexcept {
			return GetVorbisCommentValue(entry, n).data;
		}
	};

	return ParseReplayGainTagTemplate(info, VorbisCommentEntry{entry});
}
