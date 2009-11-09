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

/*
 * Functions for dealing with all configured (enabled) audion outputs
 * at once.
 *
 */

#ifndef OUTPUT_ALL_H
#define OUTPUT_ALL_H

#include <stdbool.h>
#include <stddef.h>

struct audio_format;
struct music_buffer;
struct music_chunk;

/**
 * Global initialization: load audio outputs from the configuration
 * file and initialize them.
 */
void
audio_output_all_init(void);

/**
 * Global finalization: free memory occupied by audio outputs.  All
 */
void
audio_output_all_finish(void);

/**
 * Returns the total number of audio output devices, including those
 * who are disabled right now.
 */
unsigned int audio_output_count(void);

/**
 * Returns the "i"th audio output device.
 */
struct audio_output *
audio_output_get(unsigned i);

/**
 * Returns the audio output device with the specified name.  Returns
 * NULL if the name does not exist.
 */
struct audio_output *
audio_output_find(const char *name);

/**
 * Checks the "enabled" flag of all audio outputs, and if one has
 * changed, commit the change.
 */
void
audio_output_all_enable_disable(void);

/**
 * Opens all audio outputs which are not disabled.
 *
 * @param audio_format the preferred audio format, or NULL to reuse
 * the previous format
 * @param buffer the #music_buffer where consumed #music_chunk objects
 * should be returned
 * @return true on success, false on failure
 */
bool
audio_output_all_open(const struct audio_format *audio_format,
		      struct music_buffer *buffer);

/**
 * Closes all audio outputs.
 */
void
audio_output_all_close(void);

/**
 * Enqueue a #music_chunk object for playing, i.e. pushes it to a
 * #music_pipe.
 *
 * @param chunk the #music_chunk object to be played
 * @return true on success, false if no audio output was able to play
 * (all closed then)
 */
bool
audio_output_all_play(struct music_chunk *chunk);

/**
 * Checks if the output devices have drained their music pipe, and
 * returns the consumed music chunks to the #music_buffer.
 *
 * @return the number of chunks to play left in the #music_pipe
 */
unsigned
audio_output_all_check(void);

/**
 * Checks if the size of the #music_pipe is below the #threshold.  If
 * not, it attempts to synchronize with all output threads, and waits
 * until another #music_chunk is finished.
 *
 * @param threshold the maximum number of chunks in the pipe
 * @return true if there are less than #threshold chunks in the pipe
 */
bool
audio_output_all_wait(unsigned threshold);

/**
 * Puts all audio outputs into pause mode.  Most implementations will
 * simply close it then.
 */
void
audio_output_all_pause(void);

/**
 * Drain all audio outputs.
 */
void
audio_output_all_drain(void);

/**
 * Try to cancel data which may still be in the device's buffers.
 */
void
audio_output_all_cancel(void);

/**
 * Returns the "elapsed_time" stamp of the most recently finished
 * chunk.  A negative value is returned when no chunk has been
 * finished yet.
 */
float
audio_output_all_get_elapsed_time(void);

#endif
