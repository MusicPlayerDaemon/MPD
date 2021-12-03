/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "PartitionConfig.hxx"
#include "Data.hxx"
#include "Domain.hxx"
#include "Parser.hxx"
#include "pcm/AudioParser.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"
#include "MusicChunk.hxx"

static constexpr
size_t MIN_BUFFER_SIZE = std::max(CHUNK_SIZE * 32,
				  64 * KILOBYTE);

static unsigned
GetBufferChunks(const ConfigData &config)
{
	size_t buffer_size = PlayerConfig::DEFAULT_BUFFER_SIZE;
	if (auto *param = config.GetParam(ConfigOption::AUDIO_BUFFER_SIZE)) {
		buffer_size = param->With([](const char *s){
			size_t result = ParseSize(s, KILOBYTE);
			if (result <= 0)
				throw FormatRuntimeError("buffer size \"%s\" is not a "
							 "positive integer", s);

			if (result < MIN_BUFFER_SIZE) {
				FmtWarning(config_domain, "buffer size {} is too small, using {} bytes instead",
					   result, MIN_BUFFER_SIZE);
				result = MIN_BUFFER_SIZE;
			}

			return result;
		});
	}

	unsigned buffer_chunks = buffer_size / CHUNK_SIZE;
	if (buffer_chunks >= 1 << 15)
		throw FormatRuntimeError("buffer size \"%lu\" is too big",
					 (unsigned long)buffer_size);

	return buffer_chunks;
}

PlayerConfig::PlayerConfig(const ConfigData &config)
	:buffer_chunks(GetBufferChunks(config)),
	 audio_format(config.With(ConfigOption::AUDIO_OUTPUT_FORMAT, [](const char *s){
		 if (s == nullptr)
			 return AudioFormat::Undefined();

		 return ParseAudioFormat(s, true);
	 })),
	 replay_gain(config),
	 mixramp_analyzer(config.GetBool(ConfigOption::MIXRAMP_ANALYZER, false))
{
}
