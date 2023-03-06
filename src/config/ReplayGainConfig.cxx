// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ReplayGainConfig.hxx"
#include "Data.hxx"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

static float
ParsePreamp(const char *s)
{
	assert(s != nullptr);

	char *endptr;
	float f = std::strtof(s, &endptr);
	if (endptr == s || *endptr != '\0')
		throw std::invalid_argument("Not a numeric value");

	if (f < -15.0f || f > 15.0f)
		throw std::invalid_argument("Number must be between -15 and 15");

	return std::pow(10.0f, f / 20.0f);
}

ReplayGainConfig::ReplayGainConfig(const ConfigData &config)
	:preamp(config.With(ConfigOption::REPLAYGAIN_PREAMP, [](const char *s){
		return s != nullptr
			? ParsePreamp(s)
			: 1.0f;
	})),
	 missing_preamp(config.With(ConfigOption::REPLAYGAIN_MISSING_PREAMP, [](const char *s){
		 return s != nullptr
			 ? ParsePreamp(s)
			 : 1.0f;
	 })),
	 limit(config.GetBool(ConfigOption::REPLAYGAIN_LIMIT,
			      ReplayGainConfig::DEFAULT_LIMIT))
{
}
