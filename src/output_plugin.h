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

#ifndef MPD_OUTPUT_PLUGIN_H
#define MPD_OUTPUT_PLUGIN_H

#include <glib.h>

#include <stdbool.h>
#include <stddef.h>

struct config_param;
struct audio_format;
struct tag;

/**
 * A plugin which controls an audio output device.
 */
struct audio_output_plugin {
	/**
	 * the plugin's name
	 */
	const char *name;

	/**
	 * Test if this plugin can provide a default output, in case
	 * none has been configured.  This method is optional.
	 */
	bool (*test_default_device)(void);

	/**
	 * Configure and initialize the device, but do not open it
	 * yet.
	 *
	 * @param param the configuration section, or NULL if there is
	 * no configuration
	 * @param error location to store the error occurring, or NULL
	 * to ignore errors
	 * @return NULL on error, or an opaque pointer to the plugin's
	 * data
	 */
	struct audio_output *(*init)(const struct config_param *param,
				     GError **error);

	/**
	 * Free resources allocated by this device.
	 */
	void (*finish)(struct audio_output *data);

	/**
	 * Enable the device.  This may allocate resources, preparing
	 * for the device to be opened.  Enabling a device cannot
	 * fail: if an error occurs during that, it should be reported
	 * by the open() method.
	 *
	 * @param error_r location to store the error occurring, or
	 * NULL to ignore errors
	 * @return true on success, false on error
	 */
	bool (*enable)(struct audio_output *data, GError **error_r);

	/**
	 * Disables the device.  It is closed before this method is
	 * called.
	 */
	void (*disable)(struct audio_output *data);

	/**
	 * Really open the device.
	 *
	 * @param audio_format the audio format in which data is going
	 * to be delivered; may be modified by the plugin
	 * @param error location to store the error occurring, or NULL
	 * to ignore errors
	 */
	bool (*open)(struct audio_output *data, struct audio_format *audio_format,
		     GError **error);

	/**
	 * Close the device.
	 */
	void (*close)(struct audio_output *data);

	/**
	 * Returns a positive number if the output thread shall delay
	 * the next call to play() or pause().  This should be
	 * implemented instead of doing a sleep inside the plugin,
	 * because this allows MPD to listen to commands meanwhile.
	 *
	 * @return the number of milliseconds to wait
	 */
	unsigned (*delay)(struct audio_output *data);

	/**
	 * Display metadata for the next chunk.  Optional method,
	 * because not all devices can display metadata.
	 */
	void (*send_tag)(struct audio_output *data, const struct tag *tag);

	/**
	 * Play a chunk of audio data.
	 *
	 * @param error location to store the error occurring, or NULL
	 * to ignore errors
	 * @return the number of bytes played, or 0 on error
	 */
	size_t (*play)(struct audio_output *data,
		       const void *chunk, size_t size,
		       GError **error);

	/**
	 * Wait until the device has finished playing.
	 */
	void (*drain)(struct audio_output *data);

	/**
	 * Try to cancel data which may still be in the device's
	 * buffers.
	 */
	void (*cancel)(struct audio_output *data);

	/**
	 * Pause the device.  If supported, it may perform a special
	 * action, which keeps the device open, but does not play
	 * anything.  Output plugins like "shout" might want to play
	 * silence during pause, so their clients won't be
	 * disconnected.  Plugins which do not support pausing will
	 * simply be closed, and have to be reopened when unpaused.
	 *
	 * @return false on error (output will be closed then), true
	 * for continue to pause
	 */
	bool (*pause)(struct audio_output *data);

	/**
	 * The mixer plugin associated with this output plugin.  This
	 * may be NULL if no mixer plugin is implemented.  When
	 * created, this mixer plugin gets the same #config_param as
	 * this audio output device.
	 */
	const struct mixer_plugin *mixer_plugin;
};

static inline bool
ao_plugin_test_default_device(const struct audio_output_plugin *plugin)
{
	return plugin->test_default_device != NULL
		? plugin->test_default_device()
		: false;
}

G_GNUC_MALLOC
struct audio_output *
ao_plugin_init(const struct audio_output_plugin *plugin,
	       const struct config_param *param,
	       GError **error);

void
ao_plugin_finish(struct audio_output *ao);

bool
ao_plugin_enable(struct audio_output *ao, GError **error_r);

void
ao_plugin_disable(struct audio_output *ao);

bool
ao_plugin_open(struct audio_output *ao, struct audio_format *audio_format,
	       GError **error);

void
ao_plugin_close(struct audio_output *ao);

G_GNUC_PURE
unsigned
ao_plugin_delay(struct audio_output *ao);

void
ao_plugin_send_tag(struct audio_output *ao, const struct tag *tag);

size_t
ao_plugin_play(struct audio_output *ao, const void *chunk, size_t size,
	       GError **error);

void
ao_plugin_drain(struct audio_output *ao);

void
ao_plugin_cancel(struct audio_output *ao);

bool
ao_plugin_pause(struct audio_output *ao);

#endif
