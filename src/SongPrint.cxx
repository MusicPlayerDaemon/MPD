// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SongPrint.hxx"
#include "song/LightSong.hxx"
#include "song/DetachedSong.hxx"
#include "TimePrint.hxx"
#include "TagPrint.hxx"
#include "client/Response.hxx"
#include "fs/Traits.hxx"
#include "time/ChronoUtil.hxx"
#include "util/StringBuffer.hxx"
#include "util/UriUtil.hxx"

#include <fmt/format.h>

#define SONG_FILE "file: "

static void
song_print_uri(Response &r, const char *uri, bool base) noexcept
{
	std::string allocated;

	if (base) {
		uri = PathTraitsUTF8::GetBase(uri);
	} else {
		allocated = uri_remove_auth(uri);
		if (!allocated.empty())
			uri = allocated.c_str();
	}

	r.Fmt(FMT_STRING(SONG_FILE "{}\n"), uri);
}

void
song_print_uri(Response &r, const LightSong &song, bool base) noexcept
{
	if (!base && song.directory != nullptr)
		r.Fmt(FMT_STRING(SONG_FILE "{}/{}\n"),
		      song.directory, song.uri);
	else
		song_print_uri(r, song.uri, base);
}

void
song_print_uri(Response &r, const DetachedSong &song, bool base) noexcept
{
	song_print_uri(r, song.GetURI(), base);
}

static void
song_print_real_uri(Response &r, const char *real_uri, bool base) noexcept
{
	std::string allocated;

	if (base) {
		real_uri = PathTraitsUTF8::GetBase(real_uri);
	} else {
		allocated = uri_remove_auth(real_uri);
		if (!allocated.empty())
			real_uri = allocated.c_str();
	}

	r.Fmt(FMT_STRING("RealUri: {}\n"), real_uri);
}

static void
song_print_real_uri(Response &r, const LightSong &song, bool base) noexcept
{
	if (song.real_uri == nullptr)
		return;

	if (!base && song.directory != nullptr)
		r.Fmt(FMT_STRING("RealUri: {}\n"),
		      song.real_uri);
	else
		song_print_real_uri(r, song.real_uri, base);
}

static void
song_print_real_uri(Response &r, const DetachedSong &song, bool base) noexcept
{
	if (song.HasRealURI())
		song_print_real_uri(r, song.GetRealURI(), base);
}

static void
PrintRange(Response &r, SongTime start_time, SongTime end_time) noexcept
{
	const unsigned start_ms = start_time.ToMS();
	const unsigned end_ms = end_time.ToMS();

	if (end_ms > 0)
		r.Fmt(FMT_STRING("Range: {}.{:03}-{}.{:03}\n"),
		      start_ms / 1000,
		      start_ms % 1000,
		      end_ms / 1000,
		      end_ms % 1000);
	else if (start_ms > 0)
		r.Fmt(FMT_STRING("Range: {}.{:03}-\n"),
		      start_ms / 1000,
		      start_ms % 1000);
}

void
song_print_info(Response &r, const LightSong &song, bool base) noexcept
{
	song_print_uri(r, song, base);

	song_print_real_uri(r, song, base);

	PrintRange(r, song.start_time, song.end_time);

	if (!IsNegative(song.mtime))
		time_print(r, "Last-Modified", song.mtime);

	if (song.audio_format.IsDefined())
		r.Fmt(FMT_STRING("Format: {}\n"), ToString(song.audio_format));

	tag_print_values(r, song.tag);

	const auto duration = song.GetDuration();
	if (!duration.IsNegative())
		r.Fmt(FMT_STRING("Time: {}\n"
				 "duration: {:1.3f}\n"),
		      duration.RoundS(),
		      duration.ToDoubleS());
}

void
song_print_info(Response &r, const DetachedSong &song, bool base) noexcept
{
	song_print_uri(r, song, base);

	song_print_real_uri(r, song, base);

	PrintRange(r, song.GetStartTime(), song.GetEndTime());

	if (!IsNegative(song.GetLastModified()))
		time_print(r, "Last-Modified", song.GetLastModified());

	if (const auto &f = song.GetAudioFormat(); f.IsDefined())
		r.Fmt(FMT_STRING("Format: {}\n"), ToString(f));

	tag_print_values(r, song.GetTag());

	const auto duration = song.GetDuration();
	if (!duration.IsNegative())
		r.Fmt(FMT_STRING("Time: {}\n"
				 "duration: {:1.3f}\n"),
		      duration.RoundS(),
		      duration.ToDoubleS());
}
