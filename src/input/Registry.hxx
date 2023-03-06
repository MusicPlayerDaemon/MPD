// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_REGISTRY_HXX
#define MPD_INPUT_REGISTRY_HXX

/**
 * NULL terminated list of all input plugins which were enabled at
 * compile time.
 */
extern const struct InputPlugin *const input_plugins[];

extern bool input_plugins_enabled[];

#define input_plugins_for_each(plugin) \
	for (const InputPlugin *plugin, \
		*const*input_plugin_iterator = &input_plugins[0]; \
		(plugin = *input_plugin_iterator) != NULL; \
		++input_plugin_iterator)

#define input_plugins_for_each_enabled(plugin) \
	input_plugins_for_each(plugin) \
		if (input_plugins_enabled[input_plugin_iterator - input_plugins])

[[gnu::pure]]
bool
HasRemoteTagScanner(const char *uri) noexcept;

#endif
