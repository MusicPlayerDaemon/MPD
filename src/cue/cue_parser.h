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

#ifndef MPD_CUE_PARSER_H
#define MPD_CUE_PARSER_H

#include "check.h"

#include <stdbool.h>

struct cue_parser *
cue_parser_new(void);

void
cue_parser_free(struct cue_parser *parser);

/**
 * Feed a text line from the CUE file into the parser.  Call
 * cue_parser_get() after this to see if a song has been finished.
 */
void
cue_parser_feed(struct cue_parser *parser, const char *line);

/**
 * Tell the parser that the end of the file has been reached.  Call
 * cue_parser_get() after this to see if a song has been finished.
 * This procedure must be done twice!
 */
void
cue_parser_finish(struct cue_parser *parser);

/**
 * Check if a song was finished by the last cue_parser_feed() or
 * cue_parser_finish() call.
 *
 * @return a song object that must be freed by the caller, or NULL if
 * no song was finished at this time
 */
struct song *
cue_parser_get(struct cue_parser *parser);

#endif
