/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "cue_parser.h"
#include "string_util.h"
#include "song.h"
#include "tag.h"

#include <assert.h>
#include <stdlib.h>

struct cue_parser {
	enum {
		/**
		 * Parsing the CUE header.
		 */
		HEADER,

		/**
		 * Parsing a "FILE ... WAVE".
		 */
		WAVE,

		/**
		 * Ignore everything until the next "FILE".
		 */
		IGNORE_FILE,

		/**
		 * Parsing a "TRACK ... AUDIO".
		 */
		TRACK,

		/**
		 * Ignore everything until the next "TRACK".
		 */
		IGNORE_TRACK,
	} state;

	struct tag *tag;

	char *filename;

	/**
	 * The song currently being edited.
	 */
	struct song *current;

	/**
	 * The previous song.  It is remembered because its end_time
	 * will be set to the current song's start time.
	 */
	struct song *previous;

	/**
	 * A song that is completely finished and can be returned to
	 * the caller via cue_parser_get().
	 */
	struct song *finished;

	/**
	 * Set to true after previous.end_time has been updated to the
	 * start time of the current song.
	 */
	bool last_updated;

	/**
	 * Tracks whether cue_parser_finish() has been called.  If
	 * true, then all remaining (partial) results will be
	 * delivered by cue_parser_get().
	 */
	bool end;
};

struct cue_parser *
cue_parser_new(void)
{
	struct cue_parser *parser = g_new(struct cue_parser, 1);
	parser->state = HEADER;
	parser->tag = tag_new();
	parser->filename = NULL;
	parser->current = NULL;
	parser->previous = NULL;
	parser->finished = NULL;
	parser->end = false;
	return parser;
}

void
cue_parser_free(struct cue_parser *parser)
{
	tag_free(parser->tag);
	g_free(parser->filename);

	if (parser->current != NULL)
		song_free(parser->current);

	if (parser->previous != NULL)
		song_free(parser->previous);

	if (parser->finished != NULL)
		song_free(parser->finished);

	g_free(parser);
}

static const char *
cue_next_word(char *p, char **pp)
{
	assert(p >= *pp);
	assert(!g_ascii_isspace(*p));

	const char *word = p;
	while (*p != 0 && !g_ascii_isspace(*p))
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
	if (end == NULL) {
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
	char *p = strchug_fast(*pp);
	if (*p == 0)
		return NULL;

	return cue_next_word(p, pp);
}

static const char *
cue_next_value(char **pp)
{
	char *p = strchug_fast(*pp);
	if (*p == 0)
		return NULL;

	if (*p == '"')
		return cue_next_quoted(p + 1, pp);
	else
		return cue_next_word(p, pp);
}

static void
cue_add_tag(struct tag *tag, enum tag_type type, char *p)
{
	const char *value = cue_next_value(&p);
	if (value != NULL)
		tag_add_item(tag, type, value);

}

static void
cue_parse_rem(char *p, struct tag *tag)
{
	const char *type = cue_next_token(&p);
	if (type == NULL)
		return;

	enum tag_type type2 = tag_name_parse_i(type);
	if (type2 != TAG_NUM_OF_ITEM_TYPES)
		cue_add_tag(tag, type2, p);
}

static struct tag *
cue_current_tag(struct cue_parser *parser)
{
	if (parser->state == HEADER)
		return parser->tag;
	else if (parser->state == TRACK)
		return parser->current->tag;
	else
		return NULL;
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

/**
 * Commit the current song.  It will be moved to "previous", so the
 * next song may soon edit its end time (using the next song's start
 * time).
 */
static void
cue_parser_commit(struct cue_parser *parser)
{
	/* the caller of this library must call cue_parser_get() often
	   enough */
	assert(parser->finished == NULL);
	assert(!parser->end);

	if (parser->current == NULL)
		return;

	parser->finished = parser->previous;
	parser->previous = parser->current;
	parser->current = NULL;
}

static void
cue_parser_feed2(struct cue_parser *parser, char *p)
{
	assert(parser != NULL);
	assert(!parser->end);
	assert(p != NULL);

	const char *command = cue_next_token(&p);
	if (command == NULL)
		return;

	if (strcmp(command, "REM") == 0) {
		struct tag *tag = cue_current_tag(parser);
		if (tag != NULL)
			cue_parse_rem(p, tag);
	} else if (strcmp(command, "PERFORMER") == 0) {
		/* MPD knows a "performer" tag, but it is not a good
		   match for this CUE tag; from the Hydrogenaudio
		   Knowledgebase: "At top-level this will specify the
		   CD artist, while at track-level it specifies the
		   track artist." */

		enum tag_type type = parser->state == TRACK
			? TAG_ARTIST
			: TAG_ALBUM_ARTIST;

		struct tag *tag = cue_current_tag(parser);
		if (tag != NULL)
			cue_add_tag(tag, type, p);
	} else if (strcmp(command, "TITLE") == 0) {
		if (parser->state == HEADER)
			cue_add_tag(parser->tag, TAG_ALBUM, p);
		else if (parser->state == TRACK)
			cue_add_tag(parser->current->tag, TAG_TITLE, p);
	} else if (strcmp(command, "FILE") == 0) {
		cue_parser_commit(parser);

		const char *filename = cue_next_value(&p);
		if (filename == NULL)
			return;

		const char *type = cue_next_token(&p);
		if (type == NULL)
			return;

		if (strcmp(type, "WAVE") != 0 &&
		    strcmp(type, "MP3") != 0 &&
		    strcmp(type, "AIFF") != 0) {
			parser->state = IGNORE_FILE;
			return;
		}

		parser->state = WAVE;
		g_free(parser->filename);
		parser->filename = g_strdup(filename);
	} else if (parser->state == IGNORE_FILE) {
		return;
	} else if (strcmp(command, "TRACK") == 0) {
		cue_parser_commit(parser);

		const char *nr = cue_next_token(&p);
		if (nr == NULL)
			return;

		const char *type = cue_next_token(&p);
		if (type == NULL)
			return;

		if (strcmp(type, "AUDIO") != 0) {
			parser->state = IGNORE_TRACK;
			return;
		}

		parser->state = TRACK;
		parser->current = song_remote_new(parser->filename);
		assert(parser->current->tag == NULL);
		parser->current->tag = tag_dup(parser->tag);
		tag_add_item(parser->current->tag, TAG_TRACK, nr);
		parser->last_updated = false;
	} else if (parser->state == IGNORE_TRACK) {
		return;
	} else if (parser->state == TRACK && strcmp(command, "INDEX") == 0) {
		const char *nr = cue_next_token(&p);
		if (nr == NULL)
			return;

		const char *position = cue_next_token(&p);
		if (position == NULL)
			return;

		int position_ms = cue_parse_position(position);
		if (position_ms < 0)
			return;

		if (!parser->last_updated && parser->previous != NULL &&
		    parser->previous->start_ms < (unsigned)position_ms) {
			parser->last_updated = true;
			parser->previous->end_ms = position_ms;
			parser->previous->tag->time =
				(parser->previous->end_ms - parser->previous->start_ms + 500) / 1000;
		}

		parser->current->start_ms = position_ms;
	}
}

void
cue_parser_feed(struct cue_parser *parser, const char *line)
{
	assert(parser != NULL);
	assert(!parser->end);
	assert(line != NULL);

	char *allocated = g_strdup(line);
	cue_parser_feed2(parser, allocated);
	g_free(allocated);
}

void
cue_parser_finish(struct cue_parser *parser)
{
	if (parser->end)
		/* has already been called, ignore */
		return;

	cue_parser_commit(parser);
	parser->end = true;
}

struct song *
cue_parser_get(struct cue_parser *parser)
{
	assert(parser != NULL);

	if (parser->finished == NULL && parser->end) {
		/* cue_parser_finish() has been called already:
		   deliver all remaining (partial) results */
		assert(parser->current == NULL);

		parser->finished = parser->previous;
		parser->previous = NULL;
	}

	struct song *song = parser->finished;
	parser->finished = NULL;
	return song;
}
