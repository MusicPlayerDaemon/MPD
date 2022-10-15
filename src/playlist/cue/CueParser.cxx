/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "CueParser.hxx"
#include "tag/ParseName.hxx"
#include "util/CharUtil.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

#include <algorithm>
#include <cassert>

using std::string_view_literals::operator""sv;

static std::string_view
cue_next_word(std::string_view &src) noexcept
{
	assert(!src.empty());
	assert(!IsWhitespaceNotNull(src.front()));

	auto end = std::find_if(src.begin(), src.end(),
				[](char ch){ return IsWhitespaceOrNull(ch); });
	auto word = src.substr(0, std::distance(src.begin(), end));
	src = src.substr(std::distance(src.begin(), end));
	return word;
}

static std::string_view
cue_next_quoted(std::string_view &src) noexcept
{
	auto [value, rest] = Split(src, '"');
	if (rest.data() == nullptr)
		/* syntax error - ignore it silently */
		return std::exchange(src, {});

	src = rest;
	return value;
}

static std::string_view
cue_next_token(std::string_view &src) noexcept
{
	src = StripLeft(src);
	if (src.empty())
		return {};

	return cue_next_word(src);
}

static std::string_view
cue_next_value(std::string_view &src) noexcept
{
	src = StripLeft(src);
	if (src.empty())
		return {};

	if (src.front() == '"') {
		src.remove_prefix(1);
		return cue_next_quoted(src);
	} else
		return cue_next_word(src);
}

static void
cue_add_tag(TagBuilder &tag, TagType type, std::string_view src) noexcept
{
	auto value = cue_next_value(src);
	if (value.data() != nullptr)
		tag.AddItem(type, value);
}

static void
cue_parse_rem(std::string_view src, TagBuilder &tag) noexcept
{
	auto type = cue_next_token(src);
	if (type.data() == nullptr)
		return;

	TagType type2 = tag_name_parse_i(type);
	if (type2 != TAG_NUM_OF_ITEM_TYPES)
		cue_add_tag(tag, type2, src);
}

TagBuilder *
CueParser::GetCurrentTag() noexcept
{
	if (state == HEADER)
		return &header_tag;
	else if (state == TRACK)
		return &song_tag;
	else
		return nullptr;
}

static bool
IsDigit(std::string_view s) noexcept
{
	return !s.empty() && IsDigitASCII(s.front());
}

static unsigned
cue_next_unsigned(std::string_view &src) noexcept
{
	if (!IsDigit(src)) {
		src = {};
		return 0;
	}

	unsigned value = 0;

	do {
		char ch = src.front();
		src.remove_prefix(1);

		value = value * 10u + unsigned(ch - '0');
	} while (IsDigit(src));

	return value;
}

static int
cue_parse_position(std::string_view src) noexcept
{
	unsigned minutes = cue_next_unsigned(src);
	if (src.empty() || src.front() != ':')
		return -1;

	src.remove_prefix(1);
	unsigned seconds = cue_next_unsigned(src);
	if (src.empty() || src.front() != ':')
		return -1;

	src.remove_prefix(1);
	unsigned long frames = cue_next_unsigned(src);
	if (src.data() == nullptr || !src.empty())
		return -1;

	return minutes * 60000 + seconds * 1000 + frames * 1000 / 75;
}

void
CueParser::Commit() noexcept
{
	/* the caller of this library must call cue_parser_get() often
	   enough */
	assert(finished == nullptr);
	assert(!end);

	if (current == nullptr)
		return;

	assert(!current->GetTag().IsDefined());
	current->SetTag(song_tag.Commit());

	finished = std::move(previous);
	previous = std::move(current);
}

void
CueParser::Feed(std::string_view src) noexcept
{
	assert(!end);

	const auto command = cue_next_token(src);
	if (command.data() == nullptr)
		return;

	if (command == "REM"sv) {
		TagBuilder *tag = GetCurrentTag();
		if (tag != nullptr)
			cue_parse_rem(src, *tag);
	} else if (command == "PERFORMER"sv) {
		/* MPD knows a "performer" tag, but it is not a good
		   match for this CUE tag; from the Hydrogenaudio
		   Knowledgebase: "At top-level this will specify the
		   CD artist, while at track-level it specifies the
		   track artist." */

		TagType type = state == TRACK
			? TAG_ARTIST
			: TAG_ALBUM_ARTIST;

		TagBuilder *tag = GetCurrentTag();
		if (tag != nullptr)
			cue_add_tag(*tag, type, src);
	} else if (command == "TITLE"sv) {
		if (state == HEADER)
			cue_add_tag(header_tag, TAG_ALBUM, src);
		else if (state == TRACK)
			cue_add_tag(song_tag, TAG_TITLE, src);
	} else if (command == "FILE"sv) {
		Commit();

		const auto new_filename = cue_next_value(src);
		if (new_filename.data() == nullptr)
			return;

		const auto type = cue_next_token(src);
		if (type.data() == nullptr)
			return;

		if (type != "WAVE"sv &&
		    type != "FLAC"sv && /* non-standard */
		    type != "MP3"sv &&
		    type != "AIFF"sv) {
			state = IGNORE_FILE;
			return;
		}

		state = WAVE;
		filename = new_filename;
	} else if (state == IGNORE_FILE) {
		return;
	} else if (command == "TRACK"sv) {
		Commit();

		const auto nr = cue_next_token(src);
		if (nr.data() == nullptr)
			return;

		const auto type = cue_next_token(src);
		if (type.data() == nullptr)
			return;

		if (type != "AUDIO"sv) {
			state = IGNORE_TRACK;
			return;
		}

		state = TRACK;
		ignore_index = false;
		current = std::make_unique<DetachedSong>(filename);
		assert(!current->GetTag().IsDefined());

		song_tag = header_tag;
		song_tag.AddItem(TAG_TRACK, nr);

	} else if (state == IGNORE_TRACK) {
		return;
	} else if (state == TRACK && command == "INDEX"sv) {
		if (ignore_index)
			return;

		const auto nr = cue_next_token(src);
		if (nr.data() == nullptr)
			return;

		const auto position = cue_next_token(src);
		if (position.data() == nullptr)
			return;

		int position_ms = cue_parse_position(position);
		if (position_ms < 0)
			return;

		if (previous != nullptr && previous->GetStartTime().ToMS() < (unsigned)position_ms)
			previous->SetEndTime(SongTime::FromMS(position_ms));

		if (current != nullptr)
			current->SetStartTime(SongTime::FromMS(position_ms));

		if (nr != "00"sv || previous == nullptr)
			ignore_index = true;
	}
}

void
CueParser::Finish() noexcept
{
	if (end)
		/* has already been called, ignore */
		return;

	Commit();
	end = true;
}

std::unique_ptr<DetachedSong>
CueParser::Get() noexcept
{
	if (finished == nullptr && end) {
		/* cue_parser_finish() has been called already:
		   deliver all remaining (partial) results */
		assert(current == nullptr);

		finished = std::move(previous);
	}

	return std::move(finished);
}
