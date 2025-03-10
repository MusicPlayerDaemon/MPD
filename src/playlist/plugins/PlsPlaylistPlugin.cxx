// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlsPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "input/TextInputStream.hxx"
#include "input/InputStream.hxx"
#include "song/DetachedSong.hxx"
#include "tag/Builder.hxx"
#include "util/ASCII.hxx"
#include "util/NumberParser.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

#include <string>

using std::string_view_literals::operator""sv;

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

	char *_line;
	while ((_line = is.ReadLine()) != nullptr) {
		std::string_view line = Strip(std::string_view{_line});

		if (line.empty() || line.front() == ';')
			continue;

		if (line.front() == '[')
			/* another section starts; we only want
			   [Playlist], so stop here */
			break;

		auto [name, value] = Split(line, '=');
		if (value.data() == nullptr)
			continue;

		name = Strip(name);
		value = Strip(value);

		if (StringIsEqualIgnoreCase(name, "NumberOfEntries"sv)) {
			if (!ParseIntegerTo(value, n_entries) || n_entries == 0)
				/* empty file - nothing remains to be
				   done */
				return true;

			if (n_entries > MAX_ENTRIES)
				n_entries = MAX_ENTRIES;
			entries.resize(n_entries);
		} else if (SkipPrefixIgnoreCase(name, "File"sv)) {
			unsigned i = 0;
			ParseIntegerTo(name, i);
			if (i >= 1 && i <= (n_entries > 0 ? n_entries : MAX_ENTRIES)) {
				if (entries.size() < i)
					entries.resize(i);
				entries[i - 1].file = value;
			}
		} else if (SkipPrefixIgnoreCase(name, "Title"sv)) {
			unsigned i = 0;
			ParseIntegerTo(name, i);
			if (i >= 1 && i <= (n_entries > 0 ? n_entries : MAX_ENTRIES)) {
				if (entries.size() < i)
					entries.resize(i);
				entries[i - 1].title = value;
			}
		} else if (SkipPrefixIgnoreCase(name, "Length"sv)) {
			unsigned i = 0;
			ParseIntegerTo(name, i);
			if (i >= 1 && i <= (n_entries > 0 ? n_entries : MAX_ENTRIES)) {
				if (entries.size() < i)
					entries.resize(i);
				ParseIntegerTo(value, entries[i - 1].length);
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
			tag.AddItem(TAG_TITLE, entry.title);

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
