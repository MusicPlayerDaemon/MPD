// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SongSave.hxx"
#include "pcm/AudioParser.hxx"
#include "db/plugins/simple/Song.hxx"
#include "song/DetachedSong.hxx"
#include "TagSave.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "io/LineReader.hxx"
#include "io/BufferedOutputStream.hxx"
#include "tag/ParseName.hxx"
#include "tag/Tag.hxx"
#include "tag/Builder.hxx"
#include "time/ChronoUtil.hxx"
#include "util/StringAPI.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringStrip.hxx"
#include "util/NumberParser.hxx"

#include <stdlib.h>

#define SONG_MTIME "mtime"
#define SONG_END "song_end"

static void
range_save(BufferedOutputStream &os, unsigned start_ms, unsigned end_ms)
{
	if (end_ms > 0)
		os.Fmt(FMT_STRING("Range: {}-{}\n"), start_ms, end_ms);
	else if (start_ms > 0)
		os.Fmt(FMT_STRING("Range: {}-\n"), start_ms);
}

void
song_save(BufferedOutputStream &os, const Song &song)
{
	os.Fmt(FMT_STRING(SONG_BEGIN "{}\n"), song.filename);

	if (!song.target.empty())
		os.Fmt(FMT_STRING("Target: {}\n"), song.target);

	range_save(os, song.start_time.ToMS(), song.end_time.ToMS());

	tag_save(os, song.tag);

	if (song.audio_format.IsDefined())
		os.Fmt(FMT_STRING("Format: {}\n"), song.audio_format);

	if (song.in_playlist)
		os.Write("InPlaylist: yes\n");

	if (!IsNegative(song.mtime))
		os.Fmt(FMT_STRING(SONG_MTIME ": {}\n"),
		       std::chrono::system_clock::to_time_t(song.mtime));
	os.Write(SONG_END "\n");
}

void
song_save(BufferedOutputStream &os, const DetachedSong &song)
{
	os.Fmt(FMT_STRING(SONG_BEGIN "{}\n"), song.GetURI());

	range_save(os, song.GetStartTime().ToMS(), song.GetEndTime().ToMS());

	tag_save(os, song.GetTag());

	if (!IsNegative(song.GetLastModified()))
		os.Fmt(FMT_STRING(SONG_MTIME ": {}\n"),
		       std::chrono::system_clock::to_time_t(song.GetLastModified()));
	os.Write(SONG_END "\n");
}

DetachedSong
song_load(LineReader &file, const char *uri,
	  std::string *target_r, bool *in_playlist_r)
{
	DetachedSong song(uri);

	TagBuilder tag;

	char *line;
	while ((line = file.ReadLine()) != nullptr &&
	       !StringIsEqual(line, SONG_END)) {
		char *colon = std::strchr(line, ':');
		if (colon == nullptr || colon == line)
			throw FmtRuntimeError("unknown line in db: {}", line);

		*colon++ = 0;
		const char *value = StripLeft(colon);

		TagType type;
		if ((type = tag_name_parse(line)) != TAG_NUM_OF_ITEM_TYPES) {
			tag.AddItem(type, value);
		} else if (StringIsEqual(line, "Time")) {
			tag.SetDuration(SignedSongTime::FromS(ParseDouble(value)));
		} else if (StringIsEqual(line, "Target")) {
			if (target_r != nullptr)
				*target_r = value;
		} else if (StringIsEqual(line, "Format")) {
			try {
				song.SetAudioFormat(ParseAudioFormat(value,
								     false));
			} catch (...) {
				/* ignore parser errors */
			}
		} else if (StringIsEqual(line, "Playlist")) {
			tag.SetHasPlaylist(StringIsEqual(value, "yes"));
		} else if (StringIsEqual(line, SONG_MTIME)) {
			song.SetLastModified(std::chrono::system_clock::from_time_t(atoi(value)));
		} else if (StringIsEqual(line, "Range")) {
			char *endptr;

			unsigned start_ms = strtoul(value, &endptr, 10);
			unsigned end_ms = *endptr == '-'
				? strtoul(endptr + 1, nullptr, 10)
				: 0;

			song.SetStartTime(SongTime::FromMS(start_ms));
			song.SetEndTime(SongTime::FromMS(end_ms));
		} else if (StringIsEqual(line, "InPlaylist")) {
			if (in_playlist_r != nullptr)
				*in_playlist_r = StringIsEqual(value, "yes");
		} else {
			throw FmtRuntimeError("unknown line in db: {}", line);
		}
	}

	song.SetTag(tag.Commit());
	return song;
}
