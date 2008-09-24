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

#ifndef OUTPUT_INTERNAL_H
#define OUTPUT_INTERNAL_H

#include "pcm_utils.h"
#include "notify.h"

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
	 * Is the device (already) open and functional?
	 */
	int open;

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

	ConvState convState;
	char *convBuffer;
	size_t convBufferLen;

	/**
	 * The thread handle, or "0" if the output thread isn't
	 * running.
	 */
	pthread_t thread;

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

	/**
	 * Result value of the command.  Generally, "0" means success.
	 */
	int result;
};

/**
 * Notify object used by the thread's client, i.e. we will send a
 * notify signal to this object, expecting the caller to wait on it.
 */
extern struct notify audio_output_client_notify;

#endif
