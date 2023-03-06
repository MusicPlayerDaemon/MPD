// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MIXER_TYPE_HXX
#define MPD_MIXER_TYPE_HXX

enum class MixerType {
	/** mixer disabled */
	NONE,

	/** "null" mixer (virtual fake) */
	NULL_,

	/** software mixer with pcm_volume() */
	SOFTWARE,

	/** hardware mixer (output's plugin) */
	HARDWARE,
};

/**
 * Parses a #MixerType setting from the configuration file.
 *
 * Throws if the string could not be parsed.
 *
 * @param input the configured string value
 * @return a #MixerType value
 */
MixerType
mixer_type_parse(const char *input);

#endif
