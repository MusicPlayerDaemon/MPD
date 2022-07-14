/*
 * Copyright 2003-2022 The Music Player Daemon Project
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
