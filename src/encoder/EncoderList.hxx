// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ENCODER_LIST_HXX
#define MPD_ENCODER_LIST_HXX

struct EncoderPlugin;

extern const EncoderPlugin *const encoder_plugins[];

#define encoder_plugins_for_each(plugin) \
	for (const EncoderPlugin *plugin, \
		*const*encoder_plugin_iterator = &encoder_plugins[0]; \
		(plugin = *encoder_plugin_iterator) != nullptr; \
		++encoder_plugin_iterator)

/**
 * Looks up an encoder plugin by its name.
 *
 * @param name the encoder name to look for
 * @return the encoder plugin with the specified name, or nullptr if none
 * was found
 */
const EncoderPlugin *
encoder_plugin_get(const char *name);

#endif
