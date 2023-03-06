// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PartitionConfig.hxx"
#include "Data.hxx"
#include "Domain.hxx"
#include "Parser.hxx"
#include "pcm/AudioParser.hxx"
#include "lib/fmt/RuntimeError.hxx"
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
				throw FmtRuntimeError("buffer size \"{}\" is not a "
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
		throw FmtRuntimeError("buffer size \"{}\" is too big",
				      buffer_size);

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
