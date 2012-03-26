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

#include "audio_format.h"

#include <assert.h>
#include <stdio.h>

void
audio_format_mask_apply(struct audio_format *af,
			const struct audio_format *mask)
{
	assert(audio_format_valid(af));
	assert(audio_format_mask_valid(mask));

	if (mask->sample_rate != 0)
		af->sample_rate = mask->sample_rate;

	if (mask->format != SAMPLE_FORMAT_UNDEFINED)
		af->format = mask->format;

	if (mask->channels != 0)
		af->channels = mask->channels;

	assert(audio_format_valid(af));
}

const char *
sample_format_to_string(enum sample_format format)
{
	switch (format) {
	case SAMPLE_FORMAT_UNDEFINED:
		return "?";

	case SAMPLE_FORMAT_S8:
		return "8";

	case SAMPLE_FORMAT_S16:
		return "16";

	case SAMPLE_FORMAT_S24_P32:
		return "24";

	case SAMPLE_FORMAT_S32:
		return "32";

	case SAMPLE_FORMAT_FLOAT:
		return "f";

	case SAMPLE_FORMAT_DSD:
		return "dsd";
	}

	/* unreachable */
	assert(false);
	return "?";
}

const char *
audio_format_to_string(const struct audio_format *af,
		       struct audio_format_string *s)
{
	assert(af != NULL);
	assert(s != NULL);

	snprintf(s->buffer, sizeof(s->buffer), "%u:%s:%u",
		 af->sample_rate, sample_format_to_string(af->format),
		 af->channels);

	return s->buffer;
}
