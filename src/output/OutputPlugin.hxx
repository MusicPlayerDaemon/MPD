// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OUTPUT_PLUGIN_HXX
#define MPD_OUTPUT_PLUGIN_HXX

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

[[gnu::malloc]] [[gnu::returns_nonnull]]
AudioOutput *
ao_plugin_init(EventLoop &event_loop,
	       const AudioOutputPlugin &plugin,
	       const ConfigBlock &block);

#endif
