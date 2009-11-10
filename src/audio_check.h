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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_AUDIO_CHECK_H
#define MPD_AUDIO_CHECK_H

#include <glib.h>
#include <stdbool.h>

struct audio_format;

/**
 * The GLib quark used for errors reported by this library.
 */
static inline GQuark
audio_format_quark(void)
{
	return g_quark_from_static_string("audio_format");
}

bool
audio_check_sample_rate(unsigned long sample_rate, GError **error_r);

bool
audio_check_sample_format(unsigned sample_format, GError **error_r);

bool
audio_check_channel_count(unsigned sample_format, GError **error_r);

/**
 * Wrapper for audio_format_init(), which checks all attributes.
 */
bool
audio_format_init_checked(struct audio_format *af, unsigned long sample_rate,
			  unsigned sample_format, unsigned channels,
			  GError **error_r);

#endif
