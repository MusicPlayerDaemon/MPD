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

#include "pcm_utils.h"
#include "notify.h"

#include <time.h>

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
	 * If not zero, the device has failed, and should not be
	 * reopened automatically before this time stamp.
	 */
	time_t reopen_after;

	/**
	 * The audio_format in which audio data is received from the
	 * player thread (which in turn receives it from the decoder).
	 */
	struct audio_format inAudioFormat;

	/**
	 * The audio_format which is really sent to the device.  This
	 * is basically reqAudioFormat (if configured) or
	 * inAudioFormat, but may have been modified by
	 * plugin->open().
	 */
	struct audio_format outAudioFormat;

	/**
	 * The audio_format which was configured.  Only set if
	 * convertAudioFormat is true.
	 */
	struct audio_format reqAudioFormat;

	struct pcm_convert_state convState;
	char *convBuffer;
	size_t convBufferLen;

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
	 * Command arguments, depending on the command.
	 */
	union {
		struct {
			const char *data;
			size_t size;
		} play;

		const struct tag *tag;
	} args;
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
