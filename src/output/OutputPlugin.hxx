/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "util/Compiler.h"

struct ConfigBlock;
class AudioOutput;
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
	 * Throws on error.
	 *
	 * @param param the configuration section, or nullptr if there is
	 * no configuration
	 */
	AudioOutput *(*init)(EventLoop &event_loop, const ConfigBlock &block);

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

gcc_malloc gcc_returns_nonnull
AudioOutput *
ao_plugin_init(EventLoop &event_loop,
	       const AudioOutputPlugin &plugin,
	       const ConfigBlock &block);

#endif
