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
