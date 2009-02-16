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

#ifndef MPD_OUTPUT_PLUGIN_H
#define MPD_OUTPUT_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>

struct audio_output;
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
	 * @param ao an opaque pointer to the audio_output structure
	 * @param audio_format the configured audio format, or NULL if
	 * none is configured
	 * @param param the configuration section, or NULL if there is
	 * no configuration
	 * @return NULL on error, or an opaque pointer to the plugin's
	 * data
	 */
	void *(*init)(struct audio_output *ao,
		      const struct audio_format *audio_format,
		      const struct config_param *param);

	/**
	 * Free resources allocated by this device.
	 */
	void (*finish)(void *data);

	/**
	 * Really open the device.
	 * @param audio_format the audio format in which data is going
	 * to be delivered; may be modified by the plugin
	 */
	bool (*open)(void *data, struct audio_format *audio_format);

	/**
	 * Close the device.
	 */
	void (*close)(void *data);

	/**
	 * Display metadata for the next chunk.  Optional method,
	 * because not all devices can display metadata.
	 */
	void (*send_tag)(void *data, const struct tag *tag);

	/**
	 * Play a chunk of audio data.
	 */
	bool (*play)(void *data, const char *playChunk, size_t size);

	/**
	 * Try to cancel data which may still be in the device's
	 * buffers.
	 */
	void (*cancel)(void *data);

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
	bool (*pause)(void *data);

	/**
	 * Control the device. Usualy used for implementing
	 * set and get mixer levels
	 */
	bool (*control)(void *data, int cmd, void *arg);
};

#endif
