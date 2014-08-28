/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "CueParser.hxx"
#include "util/Alloc.hxx"
#include "util/StringUtil.hxx"
#include "util/CharUtil.hxx"
#include "DetachedSong.hxx"
#include "tag/Tag.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

CueParser::CueParser()
	:state(HEADER),
	 current(nullptr),
	 previous(nullptr),
	 finished(nullptr),
	 end(false) {}

CueParser::~CueParser()
{
	delete current;
	delete previous;
	delete finished;
}

static const char *
cue_next_word(char *p, char **pp)
{
	assert(p >= *pp);
	assert(!IsWhitespaceNotNull(*p));

	const char *word = p;
	while (!IsWhitespaceOrNull(*p))
		++p;

	*p = 0;
	*pp = p + 1;
	return word;
}

static const char *
cue_next_quoted(char *p, char **pp)
{
	assert(p >= *pp);
	assert(p[-1] == '"');

	char *end = strchr(p, '"');
	if (end == nullptr) {
		/* syntax error - ignore it silently */
		*pp = p + strlen(p);
		return p;
	}

	*end = 0;
	*pp = end + 1;

	return p;
}

static const char *
cue_next_token(char **pp)
{
	char *p = StripLeft(*pp);
	if (*p == 0)
		return nullptr;

	return cue_next_word(p, pp);
}

static const char *
cue_next_value(char **pp)
{
	char *p = StripLeft(*pp);
	if (*p == 0)
		return nullptr;

	if (*p == '"')
		return cue_next_quoted(p + 1, pp);
	else
		return cue_next_word(p, pp);
}

static void
cue_add_tag(TagBuilder &tag, TagType type, char *p)
{
	const char *value = cue_next_value(&p);
	if (value != nullptr)
		tag.AddItem(type, value);

}

static void
cue_parse_rem(char *p, TagBuilder &tag)
{
	const char *type = cue_next_token(&p);
	if (type == nullptr)
		return;

	TagType type2 = tag_name_parse_i(type);
	if (type2 != TAG_NUM_OF_ITEM_TYPES)
		cue_add_tag(tag, type2, p);
}

TagBuilder *
CueParser::GetCurrentTag()
{
	if (state == HEADER)
		return &header_tag;
	else if (state == TRACK)
		return &song_tag;
	else
		return nullptr;
}

static int
cue_parse_position(const char *p)
{
	char *endptr;
	unsigned long minutes = strtoul(p, &endptr, 10);
	if (endptr == p || *endptr != ':')
		return -1;

	p = endptr + 1;
	unsigned long seconds = strtoul(p, &endptr, 10);
	if (endptr == p || *endptr != ':')
		return -1;

	p = endptr + 1;
	unsigned long frames = strtoul(p, &endptr, 10);
	if (endptr == p || *endptr != 0)
		return -1;

	return minutes * 60000 + seconds * 1000 + frames * 1000 / 75;
}

void
CueParser::Commit()
{
	/* the caller of this library must call cue_parser_get() often
	   enough */
	assert(finished == nullptr);
	assert(!end);

	if (current == nullptr)
		return;

	assert(!current->GetTag().IsDefined());
	current->SetTag(song_tag.Commit());

	finished = previous;
	previous = current;
	current = nullptr;
}

void
CueParser::Feed2(char *p)
{
	assert(!end);
	assert(p != nullptr);

	const char *command = cue_next_token(&p);
	if (command == nullptr)
		return;

	if (strcmp(command, "REM") == 0) {
		TagBuilder *tag = GetCurrentTag();
		if (tag != nullptr)
			cue_parse_rem(p, *tag);
	} else if (strcmp(command, "PERFORMER") == 0) {
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
			cue_add_tag(*tag, type, p);
	} else if (strcmp(command, "TITLE") == 0) {
		if (state == HEADER)
			cue_add_tag(header_tag, TAG_ALBUM, p);
		else if (state == TRACK)
			cue_add_tag(song_tag, TAG_TITLE, p);
	} else if (strcmp(command, "FILE") == 0) {
		Commit();

		const char *new_filename = cue_next_value(&p);
		if (new_filename == nullptr)
			return;

		const char *type = cue_next_token(&p);
		if (type == nullptr)
			return;

		if (strcmp(type, "WAVE") != 0 &&
		    strcmp(type, "MP3") != 0 &&
		    strcmp(type, "AIFF") != 0) {
			state = IGNORE_FILE;
			return;
		}

		state = WAVE;
		filename = new_filename;
	} else if (state == IGNORE_FILE) {
		return;
	} else if (strcmp(command, "TRACK") == 0) {
		Commit();

		const char *nr = cue_next_token(&p);
		if (nr == nullptr)
			return;

		const char *type = cue_next_token(&p);
		if (type == nullptr)
			return;

		if (strcmp(type, "AUDIO") != 0) {
			state = IGNORE_TRACK;
			return;
		}

		state = TRACK;
		current = new DetachedSong(filename);
		assert(!current->GetTag().IsDefined());

		song_tag = header_tag;
		song_tag.AddItem(TAG_TRACK, nr);

		last_updated = false;
	} else if (state == IGNORE_TRACK) {
		return;
	} else if (state == TRACK && strcmp(command, "INDEX") == 0) {
		const char *nr = cue_next_token(&p);
		if (nr == nullptr)
			return;

		const char *position = cue_next_token(&p);
		if (position == nullptr)
			return;

		int position_ms = cue_parse_position(position);
		if (position_ms < 0)
			return;

		if (!last_updated && previous != nullptr &&
		    previous->GetStartTime().ToMS() < (unsigned)position_ms) {
			last_updated = true;
			previous->SetEndTime(SongTime::FromMS(position_ms));
		}

		current->SetStartTime(SongTime::FromMS(position_ms));
	}
}

void
CueParser::Feed(const char *line)
{
	assert(!end);
	assert(line != nullptr);

	char *allocated = xstrdup(line);
	Feed2(allocated);
	free(allocated);
}

void
CueParser::Finish()
{
	if (end)
		/* has already been called, ignore */
		return;

	Commit();
	end = true;
}

DetachedSong *
CueParser::Get()
{
	if (finished == nullptr && end) {
		/* cue_parser_finish() has been called already:
		   deliver all remaining (partial) results */
		assert(current == nullptr);

		finished = previous;
		previous = nullptr;
	}

	DetachedSong *song = finished;
	finished = nullptr;
	return song;
}
