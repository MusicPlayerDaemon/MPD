/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include <chrono>

#include <stddef.h>

struct ConfigBlock;
struct AudioFormat;
struct Tag;
struct FilteredAudioOutput;
struct MixerPlugin;
class EventLoop;

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
	bool (*test_default_device)();

	/**
	 * Configure and initialize the device, but do not open it
	 * yet.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param param the configuration section, or nullptr if there is
	 * no configuration
	 */
	FilteredAudioOutput *(*init)(EventLoop &event_loop, const ConfigBlock &block);

	/**
	 * Free resources allocated by this device.
	 */
	void (*finish)(FilteredAudioOutput *data);

	/**
	 * Enable the device.  This may allocate resources, preparing
	 * for the device to be opened.
	 *
	 * Throws #std::runtime_error on error.
	 */
	void (*enable)(FilteredAudioOutput *data);

	/**
	 * Disables the device.  It is closed before this method is
	 * called.
	 */
	void (*disable)(FilteredAudioOutput *data);

	/**
	 * Really open the device.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param audio_format the audio format in which data is going
	 * to be delivered; may be modified by the plugin
	 */
	void (*open)(FilteredAudioOutput *data, AudioFormat &audio_format);

	/**
	 * Close the device.
	 */
	void (*close)(FilteredAudioOutput *data);

	/**
	 * Returns a positive number if the output thread shall further
	 * delay the next call to play() or pause(), which will happen
	 * until this function returns 0.  This should be implemented
	 * instead of doing a sleep inside the plugin, because this
	 * allows MPD to listen to commands meanwhile.
	 *
	 * @return the duration to wait
	 */
	std::chrono::steady_clock::duration (*delay)(FilteredAudioOutput *data) noexcept;

	/**
	 * Display metadata for the next chunk.  Optional method,
	 * because not all devices can display metadata.
	 */
	void (*send_tag)(FilteredAudioOutput *data, const Tag &tag);

	/**
	 * Play a chunk of audio data.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @return the number of bytes played
	 */
	size_t (*play)(FilteredAudioOutput *data,
		       const void *chunk, size_t size);

	/**
	 * Wait until the device has finished playing.
	 */
	void (*drain)(FilteredAudioOutput *data);

	/**
	 * Try to cancel data which may still be in the device's
	 * buffers.
	 */
	void (*cancel)(FilteredAudioOutput *data);

	/**
	 * Pause the device.  If supported, it may perform a special
	 * action, which keeps the device open, but does not play
	 * anything.  Output plugins like "shout" might want to play
	 * silence during pause, so their clients won't be
	 * disconnected.  Plugins which do not support pausing will
	 * simply be closed, and have to be reopened when unpaused.
	 *
	 * @return false on error (output will be closed by caller),
	 * true for continue to pause
	 */
	bool (*pause)(FilteredAudioOutput *data);

	/**
	 * The mixer plugin associated with this output plugin.  This
	 * may be nullptr if no mixer plugin is implemented.  When
	 * created, this mixer plugin gets the same #ConfigParam as
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
FilteredAudioOutput *
ao_plugin_init(EventLoop &event_loop,
	       const AudioOutputPlugin &plugin,
	       const ConfigBlock &block);

void
ao_plugin_finish(FilteredAudioOutput *ao) noexcept;

void
ao_plugin_enable(FilteredAudioOutput &ao);

void
ao_plugin_disable(FilteredAudioOutput &ao) noexcept;

void
ao_plugin_open(FilteredAudioOutput &ao, AudioFormat &audio_format);

void
ao_plugin_close(FilteredAudioOutput &ao) noexcept;

gcc_pure
std::chrono::steady_clock::duration
ao_plugin_delay(FilteredAudioOutput &ao) noexcept;

void
ao_plugin_send_tag(FilteredAudioOutput &ao, const Tag &tag);

size_t
ao_plugin_play(FilteredAudioOutput &ao, const void *chunk, size_t size);

void
ao_plugin_drain(FilteredAudioOutput &ao);

void
ao_plugin_cancel(FilteredAudioOutput &ao) noexcept;

bool
ao_plugin_pause(FilteredAudioOutput &ao);

#endif
