/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "ReplayGainGlobal.hxx"
#include "ReplayGainConfig.hxx"
#include "config/Param.hxx"
#include "config/Data.hxx"
#include "util/RuntimeError.hxx"

#include <assert.h>
#include <stdlib.h>
#include <math.h>

static float
ParsePreamp(const char *s)
{
	assert(s != nullptr);

	char *endptr;
	float f = strtod(s, &endptr);
	if (endptr == s || *endptr != '\0')
		throw std::invalid_argument("Not a numeric value");

	if (f < -15 || f > 15)
		throw std::invalid_argument("Number must be between -15 and 15");

	return pow(10, f / 20.0);
}

static float
ParsePreamp(const ConfigParam &p)
{
	try {
		return ParsePreamp(p.value.c_str());
	} catch (...) {
		std::throw_with_nested(FormatRuntimeError("Failed to parse line %i",
							  p.line));
	}
}

ReplayGainConfig
LoadReplayGainConfig(const ConfigData &config)
{
	ReplayGainConfig replay_gain_config;

	const auto *param = config.GetParam(ConfigOption::REPLAYGAIN_PREAMP);
	if (param)
		replay_gain_config.preamp = ParsePreamp(*param);

	param = config.GetParam(ConfigOption::REPLAYGAIN_MISSING_PREAMP);
	if (param)
		replay_gain_config.missing_preamp = ParsePreamp(*param);

	replay_gain_config.limit = config.GetBool(ConfigOption::REPLAYGAIN_LIMIT,
						  ReplayGainConfig::DEFAULT_LIMIT);

	return replay_gain_config;
}
