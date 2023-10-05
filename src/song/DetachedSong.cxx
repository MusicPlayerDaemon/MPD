// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "util/UriExtract.hxx"
#include "fs/Traits.hxx"

DetachedSong::DetachedSong(const LightSong &other) noexcept
	:uri(other.GetURI()),
	 real_uri(other.real_uri != nullptr ? other.real_uri : ""),
	 tag(other.tag),
	 mtime(other.mtime),
	 added(other.added),
	 start_time(other.start_time),
	 end_time(other.end_time),
	 audio_format(other.audio_format) {}

DetachedSong::operator LightSong() const noexcept
{
	LightSong result(uri.c_str(), tag);
	result.directory = nullptr;
	result.real_uri = real_uri.empty() ? nullptr : real_uri.c_str();
	result.mtime = mtime;
	result.added = added;
	result.start_time = start_time;
	result.end_time = end_time;
	return result;
}

bool
DetachedSong::IsRemote() const noexcept
{
	return uri_has_scheme(GetRealURI());
}

bool
DetachedSong::IsAbsoluteFile() const noexcept
{
	return PathTraitsUTF8::IsAbsolute(GetRealURI());
}

bool
DetachedSong::IsInDatabase() const noexcept
{
	/* here, we use GetURI() and not GetRealURI() because
	   GetRealURI() is never relative */

	const char *_uri = GetURI();
	return !PathTraitsUTF8::IsAbsoluteOrHasScheme(_uri);
}

SignedSongTime
DetachedSong::GetDuration() const noexcept
{
	SongTime a = start_time, b = end_time;
	if (!b.IsPositive()) {
		if (tag.duration.IsNegative())
			return tag.duration;

		b = SongTime(tag.duration);
	}

	return {b - a};
}
