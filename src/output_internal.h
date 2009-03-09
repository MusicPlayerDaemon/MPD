/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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

#ifndef MPD_OUTPUT_INTERNAL_H
#define MPD_OUTPUT_INTERNAL_H

#include "audio_format.h"
#include "pcm_convert.h"
#include "notify.h"

#include <time.h>

enum audio_output_command {
	AO_COMMAND_NONE = 0,
	AO_COMMAND_OPEN,
	AO_COMMAND_CLOSE,
	AO_COMMAND_PAUSE,
	AO_COMMAND_CANCEL,
	AO_COMMAND_KILL
};

struct audio_output {
	/**
	 * The device's configured display name.
	 */
	const char *name;

	/**
	 * The plugin which implements this output device.
	 */
	const struct audio_output_plugin *plugin;

	/**
	 * The plugin's internal data.  It is passed to every plugin
	 * method.
	 */
	void *data;

	/**
	 * Has the user enabled this device?
	 */
	bool enabled;

	/**
	 * Is the device (already) open and functional?
	 */
	bool open;

	/**
	 * If not NULL, the device has failed, and this timer is used
	 * to estimate how long it should stay disabled (unless
	 * explicitly reopened with "play").
	 */
	GTimer *fail_timer;

	/**
	 * The audio_format in which audio data is received from the
	 * player thread (which in turn receives it from the decoder).
	 */
	struct audio_format in_audio_format;

	/**
	 * The audio_format which is really sent to the device.  This
	 * is basically config_audio_format (if configured) or
	 * in_audio_format, but may have been modified by
	 * plugin->open().
	 */
	struct audio_format out_audio_format;

	/**
	 * The audio_format which was configured.  Only set if
	 * convertAudioFormat is true.
	 */
	struct audio_format config_audio_format;

	struct pcm_convert_state convert_state;

	/**
	 * The thread handle, or NULL if the output thread isn't
	 * running.
	 */
	GThread *thread;

	/**
	 * Notify object for the thread.
	 */
	struct notify notify;

	/**
	 * The next command to be performed by the output thread.
	 */
	enum audio_output_command command;

	/**
	 * The music pipe which provides music chunks to be played.
	 */
	const struct music_pipe *pipe;

	/**
	 * This mutex protects #chunk and #chunk_finished.
	 */
	GMutex *mutex;

	/**
	 * The #music_chunk which is currently being played.  All
	 * chunks before this one may be returned to the
	 * #music_buffer, because they are not going to be used by
	 * this output anymore.
	 */
	const struct music_chunk *chunk;

	/**
	 * Has the output finished playing #chunk?
	 */
	bool chunk_finished;
};

/**
 * Notify object used by the thread's client, i.e. we will send a
 * notify signal to this object, expecting the caller to wait on it.
 */
extern struct notify audio_output_client_notify;

static inline bool
audio_output_is_open(const struct audio_output *ao)
{
	return ao->open;
}

static inline bool
audio_output_command_is_finished(const struct audio_output *ao)
{
	return ao->command == AO_COMMAND_NONE;
}

#endif
