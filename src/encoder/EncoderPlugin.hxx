// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ENCODER_PLUGIN_HXX
#define MPD_ENCODER_PLUGIN_HXX

class PreparedEncoder;
struct ConfigBlock;

struct EncoderPlugin {
	const char *name;

	/**
	 * Throws #std::runtime_error on error.
	 */
	PreparedEncoder *(*init)(const ConfigBlock &block);
};

/**
 * Creates a new encoder object.
 *
 * Throws #std::runtime_error on error.
 *
 * @param plugin the encoder plugin
 */
static inline PreparedEncoder *
encoder_init(const EncoderPlugin &plugin, const ConfigBlock &block)
{
	return plugin.init(block);
}

#endif
