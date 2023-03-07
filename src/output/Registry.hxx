// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OUTPUT_LIST_HXX
#define MPD_OUTPUT_LIST_HXX

struct AudioOutputPlugin;

extern const AudioOutputPlugin *const audio_output_plugins[];

[[gnu::pure]]
const AudioOutputPlugin *
GetAudioOutputPluginByName(const char *name) noexcept;

#define audio_output_plugins_for_each(plugin) \
	for (const AudioOutputPlugin *plugin, \
		*const*output_plugin_iterator = &audio_output_plugins[0]; \
		(plugin = *output_plugin_iterator) != nullptr; ++output_plugin_iterator)

#endif
