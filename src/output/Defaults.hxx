// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_AUDIO_OUTPUT_DEFAULTS_HXX
#define MPD_AUDIO_OUTPUT_DEFAULTS_HXX

#include "mixer/Type.hxx"

struct ConfigData;

/**
 * This struct contains global AudioOutput configuration settings
 * which may provide defaults for per-output settings.
 */
struct AudioOutputDefaults {
	bool normalize = false;

	MixerType mixer_type = MixerType::HARDWARE;

	constexpr AudioOutputDefaults() = default;

	/**
	 * Load defaults from configuration file.
	 *
	 * Throws on error.
	 */
	explicit AudioOutputDefaults(const ConfigData &config);
};

#endif
