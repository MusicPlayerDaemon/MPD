// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONFIGURED_RESAMPLER_HXX
#define MPD_CONFIGURED_RESAMPLER_HXX

struct ConfigData;
class PcmResampler;

void
pcm_resampler_global_init(const ConfigData &config);

/**
 * Create a #PcmResampler instance from the implementation class
 * configured in mpd.conf.
 */
PcmResampler *
pcm_resampler_create();

#endif
