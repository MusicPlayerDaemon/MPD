// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ReplayGainInfo.hxx"
#include "config/ReplayGainConfig.hxx"

#include <cmath>

float
ReplayGainTuple::CalculateScale(const ReplayGainConfig &config) const noexcept
{
	float scale;

	if (IsDefined()) {
		scale = std::pow(10.0f, gain / 20.0f);
		scale *= config.preamp;
		if (scale > 15.0f)
			scale = 15.0f;

		if (config.limit && scale * peak > 1.0f)
			scale = 1.0f / peak;
	} else
		scale = config.missing_preamp;

	return scale;
}
