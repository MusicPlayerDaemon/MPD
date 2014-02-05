/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_OUTPUT_PLUGIN_HXX
#define MPD_OUTPUT_PLUGIN_HXX

#include "Compiler.h"

#include <stddef.h>

struct config_param;
struct AudioFormat;
struct Tag;
struct AudioOutput;
struct MixerPlugin;
class Error;

/**
 * A plugin which controls an audio output device.
 */
struct AudioOutputPlugin {
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
	 * @param param the configuration section, or nullptr if there is
	 * no configuration
	 * @return nullptr on error, or an opaque pointer to the plugin's
	 * data
	 */
	AudioOutput *(*init)(const config_param &param,
				     Error &error);

	/**
	 * Free resources allocated by this device.
	 */
	void (*finish)(AudioOutput *data);

	/**
	 * Enable the device.  This may allocate resources, preparing
	 * for the device to be opened.  Enabling a device cannot
	 * fail: if an error occurs during that, it should be reported
	 * by the open() method.
	 *
	 * @return true on success, false on error
	 */
	bool (*enable)(AudioOutput *data, Error &error);

	/**
	 * Disables the device.  It is closed before this method is
	 * called.
	 */
	void (*disable)(AudioOutput *data);

	/**
	 * Really open the device.
	 *
	 * @param audio_format the audio format in which data is going
	 * to be delivered; may be modified by the plugin
	 */
	bool (*open)(AudioOutput *data, AudioFormat &audio_format,
		     Error &error);

	/**
	 * Close the device.
	 */
	void (*close)(AudioOutput *data);

	/**
	 * Returns a positive number if the output thread shall delay
	 * the next call to play() or pause().  This should be
	 * implemented instead of doing a sleep inside the plugin,
	 * because this allows MPD to listen to commands meanwhile.
	 *
	 * @return the number of milliseconds to wait
	 */
	unsigned (*delay)(AudioOutput *data);

	/**
	 * Display metadata for the next chunk.  Optional method,
	 * because not all devices can display metadata.
	 */
	void (*send_tag)(AudioOutput *data, const Tag *tag);

	/**
	 * Play a chunk of audio data.
	 *
	 * @return the number of bytes played, or 0 on error
	 */
	size_t (*play)(AudioOutput *data,
		       const void *chunk, size_t size,
		       Error &error);

	/**
	 * Wait until the device has finished playing.
	 */
	void (*drain)(AudioOutput *data);

	/**
	 * Try to cancel data which may still be in the device's
	 * buffers.
	 */
	void (*cancel)(AudioOutput *data);

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
	bool (*pause)(AudioOutput *data);

	/**
	 * The mixer plugin associated with this output plugin.  This
	 * may be nullptr if no mixer plugin is implemented.  When
	 * created, this mixer plugin gets the same #config_param as
	 * this audio output device.
	 */
	const MixerPlugin *mixer_plugin;
};

static inline bool
ao_plugin_test_default_device(const AudioOutputPlugin *plugin)
{
	return plugin->test_default_device != nullptr
		? plugin->test_default_device()
		: false;
}

gcc_malloc
AudioOutput *
ao_plugin_init(const AudioOutputPlugin *plugin,
	       const config_param &param,
	       Error &error);

void
ao_plugin_finish(AudioOutput *ao);

bool
ao_plugin_enable(AudioOutput *ao, Error &error);

void
ao_plugin_disable(AudioOutput *ao);

bool
ao_plugin_open(AudioOutput *ao, AudioFormat &audio_format,
	       Error &error);

void
ao_plugin_close(AudioOutput *ao);

gcc_pure
unsigned
ao_plugin_delay(AudioOutput *ao);

void
ao_plugin_send_tag(AudioOutput *ao, const Tag *tag);

size_t
ao_plugin_play(AudioOutput *ao, const void *chunk, size_t size,
	       Error &error);

void
ao_plugin_drain(AudioOutput *ao);

void
ao_plugin_cancel(AudioOutput *ao);

bool
ao_plugin_pause(AudioOutput *ao);

#endif
