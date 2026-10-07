// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "pcm/AudioFormat.hxx"
#include "util/ByteSizes.hxx"
#include "ReplayGainConfig.hxx"

struct ConfigData;

struct PlayerConfig {
	static constexpr size_t DEFAULT_BUFFER_SIZE = 8_MiB;

	unsigned buffer_chunks = DEFAULT_BUFFER_SIZE;

	/**
	 * The "audio_output_format" setting.
	 */
	AudioFormat audio_format = AudioFormat::Undefined();

	ReplayGainConfig replay_gain;

	bool mixramp_analyzer = false;

	PlayerConfig() = default;

	explicit PlayerConfig(const ConfigData &config);
};
