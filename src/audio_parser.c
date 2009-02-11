/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Parser functions for audio related objects.
 *
 */

#include "audio_parser.h"
#include "audio_format.h"

#include <stdlib.h>

/**
 * The GLib quark used for errors reported by this library.
 */
static inline GQuark
audio_parser_quark(void)
{
	return g_quark_from_static_string("audio_parser");
}

bool
audio_format_parse(struct audio_format *dest, const char *src, GError **error)
{
	char *endptr;
	unsigned long value;

	audio_format_clear(dest);

	/* parse sample rate */

	value = strtoul(src, &endptr, 10);
	if (endptr == src) {
		g_set_error(error, audio_parser_quark(), 0,
			    "Sample rate missing");
		return false;
	} else if (*endptr != ':') {
		g_set_error(error, audio_parser_quark(), 0,
			    "Sample format missing");
		return false;
	} else if (value <= 0 || value > G_MAXINT32) {
		g_set_error(error, audio_parser_quark(), 0,
			    "Invalid sample rate: %lu", value);
		return false;
	}

	dest->sample_rate = value;

	/* parse sample format */

	src = endptr + 1;
	value = strtoul(src, &endptr, 10);
	if (endptr == src) {
		g_set_error(error, audio_parser_quark(), 0,
			    "Sample format missing");
		return false;
	} else if (*endptr != ':') {
		g_set_error(error, audio_parser_quark(), 0,
			    "Channel count missing");
		return false;
	} else if (value != 16 && value != 24 && value != 8) {
		g_set_error(error, audio_parser_quark(), 0,
			    "Invalid sample format: %lu", value);
		return false;
	}

	dest->bits = value;

	/* parse channel count */

	src = endptr + 1;
	value = strtoul(src, &endptr, 10);
	if (*endptr != 0 || (value != 1 && value != 2)) {
		g_set_error(error, audio_parser_quark(), 0,
			    "Invalid channel count: %s", src);
		return false;
	}

	dest->channels = value;

	return true;
}
