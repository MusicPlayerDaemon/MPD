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

#ifndef MPD_OUTPUT_CONTROL_H
#define MPD_OUTPUT_CONTROL_H

#include <glib.h>

#include <stddef.h>
#include <stdbool.h>

struct audio_output;
struct audio_format;
struct config_param;
struct music_pipe;

static inline GQuark
audio_output_quark(void)
{
	return g_quark_from_static_string("audio_output");
}

bool
audio_output_init(struct audio_output *ao, const struct config_param *param,
		  GError **error_r);

/**
 * Enables the device.
 */
void
audio_output_enable(struct audio_output *ao);

/**
 * Disables the device.
 */
void
audio_output_disable(struct audio_output *ao);

/**
 * Opens or closes the device, depending on the "enabled" flag.
 *
 * @return true if the device is open
 */
bool
audio_output_update(struct audio_output *ao,
		    const struct audio_format *audio_format,
		    const struct music_pipe *mp);

void
audio_output_play(struct audio_output *ao);

void audio_output_pause(struct audio_output *ao);

void
audio_output_drain_async(struct audio_output *ao);

void audio_output_cancel(struct audio_output *ao);
void audio_output_close(struct audio_output *ao);
void audio_output_finish(struct audio_output *ao);

#endif
