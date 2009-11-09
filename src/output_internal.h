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

#ifndef MPD_OUTPUT_INTERNAL_H
#define MPD_OUTPUT_INTERNAL_H

#include "audio_format.h"

#include <glib.h>

#include <time.h>

enum audio_output_command {
	AO_COMMAND_NONE = 0,
	AO_COMMAND_ENABLE,
	AO_COMMAND_DISABLE,
	AO_COMMAND_OPEN,

	/**
	 * This command is invoked when the input audio format
	 * changes.
	 */
	AO_COMMAND_REOPEN,

	AO_COMMAND_CLOSE,
	AO_COMMAND_PAUSE,

	/**
	 * Drains the internal (hardware) buffers of the device.  This
	 * operation may take a while to complete.
	 */
	AO_COMMAND_DRAIN,

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
	 * The #mixer object associated with this audio output device.
	 * May be NULL if none is available, or if software volume is
	 * configured.
	 */
	struct mixer *mixer;

	/**
	 * Has the user enabled this device?
	 */
	bool enabled;

	/**
	 * Is this device actually enabled, i.e. the "enable" method
	 * has succeeded?
	 */
	bool really_enabled;

	/**
	 * Is the device (already) open and functional?
	 *
	 * This attribute may only be modified by the output thread.
	 * It is protected with #mutex: write accesses inside the
	 * output thread and read accesses outside of it may only be
	 * performed while the lock is held.
	 */
	bool open;

	/**
	 * Is the device paused?  i.e. the output thread is in the
	 * ao_pause() loop.
	 */
	bool pause;

	/**
	 * If not NULL, the device has failed, and this timer is used
	 * to estimate how long it should stay disabled (unless
	 * explicitly reopened with "play").
	 */
	GTimer *fail_timer;

	/**
	 * The configured audio format.
	 */
	struct audio_format config_audio_format;

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
	 * The filter object of this audio output.  This is an
	 * instance of chain_filter_plugin.
	 */
	struct filter *filter;

	/**
	 * The convert_filter_plugin instance of this audio output.
	 * It is the last item in the filter chain, and is responsible
	 * for converting the input data into the appropriate format
	 * for this audio output.
	 */
	struct filter *convert_filter;

	/**
	 * The thread handle, or NULL if the output thread isn't
	 * running.
	 */
	GThread *thread;

	/**
	 * The next command to be performed by the output thread.
	 */
	enum audio_output_command command;

	/**
	 * The music pipe which provides music chunks to be played.
	 */
	const struct music_pipe *pipe;

	/**
	 * This mutex protects #open, #chunk and #chunk_finished.
	 */
	GMutex *mutex;

	/**
	 * This condition object wakes up the output thread after
	 * #command has been set.
	 */
	GCond *cond;

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
