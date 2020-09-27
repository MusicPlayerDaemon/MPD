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

#include "PlsPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "input/TextInputStream.hxx"
#include "input/InputStream.hxx"
#include "song/DetachedSong.hxx"
#include "tag/Builder.hxx"
#include "util/ASCII.hxx"
#include "util/StringStrip.hxx"
#include "util/DivideString.hxx"

#include <string>

#include <stdlib.h>

static bool
FindPlaylistSection(TextInputStream &is)
{
	char *line;
	while ((line = is.ReadLine()) != nullptr) {
		line = Strip(line);
		if (StringEqualsCaseASCII(line, "[playlist]"))
			return true;
	}

	return false;
}

static bool
ParsePls(TextInputStream &is, std::forward_list<DetachedSong> &songs)
{
	assert(songs.empty());

	if (!FindPlaylistSection(is))
		return false;

	unsigned n_entries = 0;

	struct Entry {
		std::string file, title;
		int length{-1};

		Entry() = default;
	};

	static constexpr unsigned MAX_ENTRIES = 65536;

	std::vector<Entry> entries;

	char *line;
	while ((line = is.ReadLine()) != nullptr) {
		line = Strip(line);

		if (*line == 0 || *line == ';')
			continue;

		if (*line == '[')
			/* another section starts; we only want
			   [Playlist], so stop here */
			break;

		const DivideString ds(line, '=', true);
		if (!ds.IsDefined())
			continue;

		const char *const name = ds.GetFirst();
		const char *const value = ds.GetSecond();

		if (StringEqualsCaseASCII(name, "NumberOfEntries")) {
			n_entries = strtoul(value, nullptr, 10);
			if (n_entries == 0)
				/* empty file - nothing remains to be
				   done */
				return true;

			if (n_entries > MAX_ENTRIES)
				n_entries = MAX_ENTRIES;
			entries.resize(n_entries);
		} else if (StringEqualsCaseASCII(name, "File", 4)) {
			unsigned i = strtoul(name + 4, nullptr, 10);
			if (i >= 1 && i <= (n_entries > 0 ? n_entries : MAX_ENTRIES)) {
				if (entries.size() < i)
					entries.resize(i);
				entries[i - 1].file = value;
			}
		} else if (StringEqualsCaseASCII(name, "Title", 5)) {
			unsigned i = strtoul(name + 5, nullptr, 10);
			if (i >= 1 && i <= (n_entries > 0 ? n_entries : MAX_ENTRIES)) {
				if (entries.size() < i)
					entries.resize(i);
				entries[i - 1].title = value;
			}
		} else if (StringEqualsCaseASCII(name, "Length", 6)) {
			unsigned i = strtoul(name + 6, nullptr, 10);
			if (i >= 1 && i <= (n_entries > 0 ? n_entries : MAX_ENTRIES)) {
				if (entries.size() < i)
					entries.resize(i);
				entries[i - 1].length = atoi(value);
			}
		}
	}

	if (n_entries == 0)
		/* no "NumberOfEntries" found */
		return false;

	auto i = songs.before_begin();
	for (const auto &entry : entries) {
		const char *uri = entry.file.c_str();

		TagBuilder tag;
		if (!entry.title.empty())
			tag.AddItem(TAG_TITLE, entry.title.c_str());

		if (entry.length > 0)
			tag.SetDuration(SignedSongTime::FromS(entry.length));

		i = songs.emplace_after(i, uri, tag.Commit());
	}

	return true;
}

static bool
ParsePls(InputStreamPtr &&is, std::forward_list<DetachedSong> &songs)
{
	TextInputStream tis(std::move(is));
	if (!ParsePls(tis, songs)) {
		is = tis.StealInputStream();
		return false;
	}

	return true;
}

static std::unique_ptr<SongEnumerator>
pls_open_stream(InputStreamPtr &&is)
{
	std::forward_list<DetachedSong> songs;
	if (!ParsePls(std::move(is), songs))
		return nullptr;

	return std::make_unique<MemorySongEnumerator>(std::move(songs));
}

static constexpr const char *pls_suffixes[] = {
	"pls",
	nullptr
};

static constexpr const char *pls_mime_types[] = {
	"audio/x-scpls",
	nullptr
};

const PlaylistPlugin pls_playlist_plugin =
	PlaylistPlugin("pls", pls_open_stream)
	.WithSuffixes(pls_suffixes)
	.WithMimeTypes(pls_mime_types);
