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

#include "Id3MixRamp.hxx"
#include "MixRampParser.hxx"
#include "MixRampInfo.hxx"

#include <id3tag.h>

#include <stdlib.h>

MixRampInfo
Id3ToMixRampInfo(const struct id3_tag *tag) noexcept
{
	MixRampInfo result;

	struct id3_frame *frame;
	for (unsigned i = 0; (frame = id3_tag_findframe(tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		char *const key = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[1]));
		char *const value = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[2]));

		ParseMixRampTag(result, key, value);

		free(key);
		free(value);
	}

	return result;
}
