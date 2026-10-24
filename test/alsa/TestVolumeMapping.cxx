// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "mixer/plugins/VolumeMapping.hxx"

#include <gtest/gtest.h>

#include <cmath>

/**
 * Convert a normalized volume to dB (in 0.01 dB units), using the
 * mapping documented in VolumeMapping.cxx.
 */
static double
NormalizedToDb(double volume, long min, long max) noexcept
{
	if (max - min <= 2400)
		return volume * (max - min) + min;

	double min_norm = 0;
	if (min != SND_CTL_TLV_DB_GAIN_MUTE)
		min_norm = std::pow(10, (min - max) / 6000.0);

	return 6000 * std::log10(volume * (1 - min_norm) + min_norm) + max;
}

/**
 * Convert a linear scale factor to dB (in 0.01 dB units).
 */
static double
ScaleToDb(double scale) noexcept
{
	return 2000 * std::log10(scale);
}

TEST(VolumeMapping, LinearDb)
{
	static constexpr long min = -2400, max = 0;

	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(1, 1, min, max), 1);
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.5, 1, min, max), 0.5);

	/* -20 dB */
	EXPECT_NEAR(scale_normalized_volume_dB(1, 0.1, min, max),
		    1 - 2000.0 / 2400, 1e-9);

	for (const double volume : {1.0, 0.9, 0.5}) {
		for (const double scale : {0.9, 0.5, 0.3}) {
			const double result =
				scale_normalized_volume_dB(volume, scale,
							   min, max);
			EXPECT_NEAR(NormalizedToDb(result, min, max) -
				    NormalizedToDb(volume, min, max),
				    ScaleToDb(scale), 1e-6);
		}
	}

	/* clamp below the minimum */
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.5, 0.01, min, max), 0);
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.5, 0, min, max), 0);

	/* clamp above the maximum */
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.9, 2, min, max), 1);
}

TEST(VolumeMapping, CubicMute)
{
	static constexpr long min = SND_CTL_TLV_DB_GAIN_MUTE, max = 0;

	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(1, 1, min, max), 1);

	/* -10 dB is the cubic root of 10^(-0.5), not 10^(-0.5) */
	EXPECT_NEAR(scale_normalized_volume_dB(1, std::pow(10, -0.5),
					       min, max),
		    std::cbrt(std::pow(10, -0.5)), 1e-9);

	for (const double volume : {1.0, 0.9, 0.5}) {
		for (const double scale : {0.9, 0.5, 0.1, 0.01}) {
			const double result =
				scale_normalized_volume_dB(volume, scale,
							   min, max);
			EXPECT_NEAR(NormalizedToDb(result, min, max) -
				    NormalizedToDb(volume, min, max),
				    ScaleToDb(scale), 1e-6);
		}
	}

	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.5, 0, min, max), 0);
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.9, 2, min, max), 1);
}

TEST(VolumeMapping, Cubic)
{
	static constexpr long min = -5100, max = 600;

	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(1, 1, min, max), 1);

	for (const double volume : {1.0, 0.9, 0.5}) {
		for (const double scale : {0.9, 0.5, 0.1}) {
			const double result =
				scale_normalized_volume_dB(volume, scale,
							   min, max);
			EXPECT_NEAR(NormalizedToDb(result, min, max) -
				    NormalizedToDb(volume, min, max),
				    ScaleToDb(scale), 1e-6);
		}
	}

	/* clamp below the minimum */
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.5, 0.001, min, max), 0);
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.5, 0, min, max), 0);

	/* clamp above the maximum */
	EXPECT_DOUBLE_EQ(scale_normalized_volume_dB(0.9, 2, min, max), 1);
}
