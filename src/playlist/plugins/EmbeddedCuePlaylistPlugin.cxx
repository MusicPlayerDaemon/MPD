// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
#include "util/StringCompare.hxx"

#include <memory>

#include <string.h>

using std::string_view_literals::operator""sv;

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

	void OnPair(std::string_view key, std::string_view value) noexcept override;
};

void
ExtractCuesheetTagHandler::OnPair(std::string_view name, std::string_view value) noexcept
{
	if (cuesheet.empty() && StringIsEqualIgnoreCase(name, "cuesheet"sv))
		cuesheet = value;
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

	playlist->next = playlist->cuesheet.data();
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
