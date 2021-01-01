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

/** \file
 *
 * Playlist plugin that reads embedded cue sheets from the "CUESHEET"
 * tag of a music file.
 */

#include "EmbeddedCuePlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "../cue/CueParser.hxx"
#include "tag/Handler.hxx"
#include "tag/Generic.hxx"
#include "song/DetachedSong.hxx"
#include "TagFile.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/StringView.hxx"

#include <memory>

#include <string.h>

class EmbeddedCuePlaylist final : public SongEnumerator {
public:
	/**
	 * This is an override for the CUE's "FILE".  An embedded CUE
	 * sheet must always point to the song file it is contained
	 * in.
	 */
	std::string filename;

	/**
	 * The value of the file's "CUESHEET" tag.
	 */
	std::string cuesheet;

	/**
	 * The offset of the next line within "cuesheet".
	 */
	char *next;

	std::unique_ptr<CueParser> parser;

	std::unique_ptr<DetachedSong> NextSong() override;
};

class ExtractCuesheetTagHandler final : public NullTagHandler {
public:
	std::string cuesheet;

	ExtractCuesheetTagHandler() noexcept:NullTagHandler(WANT_PAIR) {}

	void OnPair(StringView key, StringView value) noexcept override;
};

void
ExtractCuesheetTagHandler::OnPair(StringView name, StringView value) noexcept
{
	if (cuesheet.empty() && name.EqualsIgnoreCase("cuesheet"))
		cuesheet = {value.data, value.size};
}

static std::unique_ptr<SongEnumerator>
embcue_playlist_open_uri(const char *uri,
			 [[maybe_unused]] Mutex &mutex)
{
	if (!PathTraitsUTF8::IsAbsolute(uri))
		/* only local files supported */
		return nullptr;

	const auto path_fs = AllocatedPath::FromUTF8Throw(uri);

	ExtractCuesheetTagHandler extract_cuesheet;
	ScanFileTagsNoGeneric(path_fs, extract_cuesheet);
	if (extract_cuesheet.cuesheet.empty())
		ScanGenericTags(path_fs, extract_cuesheet);

	if (extract_cuesheet.cuesheet.empty())
		/* no "CUESHEET" tag found */
		return nullptr;

	auto playlist = std::make_unique<EmbeddedCuePlaylist>();

	playlist->filename = PathTraitsUTF8::GetBase(uri);

	playlist->cuesheet = std::move(extract_cuesheet.cuesheet);

	playlist->next = &playlist->cuesheet[0];
	playlist->parser = std::make_unique<CueParser>();

	return playlist;
}

std::unique_ptr<DetachedSong>
EmbeddedCuePlaylist::NextSong()
{
	auto song = parser->Get();
	if (song != nullptr) {
		song->SetURI(filename);
		return song;
	}

	while (*next != 0) {
		const char *line = next;
		char *eol = strpbrk(next, "\r\n");
		if (eol != nullptr) {
			/* null-terminate the line */
			*eol = 0;
			next = eol + 1;
		} else
			/* last line; put the "next" pointer to the
			   end of the buffer */
			next += strlen(line);

		parser->Feed(line);
		song = parser->Get();
		if (song != nullptr) {
			song->SetURI(filename);
			return song;
		}
	}

	parser->Finish();
	song = parser->Get();
	if (song != nullptr)
		song->SetURI(filename);
	return song;
}

static const char *const embcue_playlist_suffixes[] = {
	/* a few codecs that are known to be supported; there are
	   probably many more */
	"flac",
	"mp3", "mp2",
	"mp4", "mp4a", "m4b",
	"ape",
	"wv",
	"ogg", "oga",
	nullptr
};

const PlaylistPlugin embcue_playlist_plugin =
	PlaylistPlugin("embcue", embcue_playlist_open_uri)
	.WithSuffixes(embcue_playlist_suffixes);
