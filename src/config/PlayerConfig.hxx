// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "pcm/AudioFormat.hxx"
#include "ReplayGainConfig.hxx"

struct ConfigData;

static constexpr size_t KILOBYTE = 1024;
static constexpr size_t MEGABYTE = 1024 * KILOBYTE;

struct PlayerConfig {
	static constexpr size_t DEFAULT_BUFFER_SIZE = 8 * MEGABYTE;

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
