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

#ifndef MPD_OUTPUT_CONTROL_H
#define MPD_OUTPUT_CONTROL_H

#include <glib.h>

#include <stddef.h>
#include <stdbool.h>

struct audio_output;
struct audio_format;
struct config_param;
struct music_pipe;
struct player_control;

static inline GQuark
audio_output_quark(void)
{
	return g_quark_from_static_string("audio_output");
}

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

/**
 * Clear the "allow_play" flag and send the "CANCEL" command
 * asynchronously.  To finish the operation, the caller has to call
 * audio_output_allow_play().
 */
void audio_output_cancel(struct audio_output *ao);

/**
 * Set the "allow_play" and signal the thread.
 */
void
audio_output_allow_play(struct audio_output *ao);

void audio_output_close(struct audio_output *ao);

/**
 * Closes the audio output, but if the "always_on" flag is set, put it
 * into pause mode instead.
 */
void
audio_output_release(struct audio_output *ao);

void audio_output_finish(struct audio_output *ao);

#endif
