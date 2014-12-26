/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "ReplayGainFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/FilterRegistry.hxx"
#include "AudioFormat.hxx"
#include "ReplayGainInfo.hxx"
#include "ReplayGainConfig.hxx"
#include "mixer/MixerControl.hxx"
#include "pcm/Volume.hxx"
#include "pcm/PcmBuffer.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

static constexpr Domain replay_gain_domain("replay_gain");

class ReplayGainFilter final : public Filter {
	/**
	 * If set, then this hardware mixer is used for applying
	 * replay gain, instead of the software volume library.
	 */
	Mixer *mixer;

	/**
	 * The base volume level for scale=1.0, between 1 and 100
	 * (including).
	 */
	unsigned base;

	ReplayGainMode mode;

	ReplayGainInfo info;

	/**
	 * About the current volume: it is between 0 and a value that
	 * may or may not exceed #PCM_VOLUME_1.
	 *
	 * If the default value of true is used for replaygain_limit, the
	 * application of the volume to the signal will never cause clipping.
	 *
	 * On the other hand, if the user has set replaygain_limit to false,
	 * the chance of clipping is explicitly preferred if that's required to
	 * maintain a consistent audio level. Whether clipping will actually
	 * occur depends on what value the user is using for replaygain_preamp.
	 */
	PcmVolume pv;

public:
	ReplayGainFilter()
		:mixer(nullptr), mode(REPLAY_GAIN_OFF) {
		info.Clear();
	}

	void SetMixer(Mixer *_mixer, unsigned _base) {
		assert(_mixer == nullptr || (_base > 0 && _base <= 100));

		mixer = _mixer;
		base = _base;

		Update();
	}

	void SetInfo(const ReplayGainInfo *_info) {
		if (_info != nullptr) {
			info = *_info;
			info.Complete();
		} else
			info.Clear();

		Update();
	}

	void SetMode(ReplayGainMode _mode) {
		if (_mode == mode)
			/* no change */
			return;

		FormatDebug(replay_gain_domain,
			    "replay gain mode has changed %d->%d\n",
			    mode, _mode);

		mode = _mode;
		Update();
	}

	/**
	 * Recalculates the new volume after a property was changed.
	 */
	void Update();

	/* virtual methods from class Filter */
	AudioFormat Open(AudioFormat &af, Error &error) override;
	void Close() override;
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src,
				    Error &error) override;
};

void
ReplayGainFilter::Update()
{
	unsigned volume = PCM_VOLUME_1;
	if (mode != REPLAY_GAIN_OFF) {
		const auto &tuple = info.tuples[mode];
		float scale = tuple.CalculateScale(replay_gain_preamp,
						   replay_gain_missing_preamp,
						   replay_gain_limit);
		FormatDebug(replay_gain_domain,
			    "scale=%f\n", (double)scale);

		volume = pcm_float_to_volume(scale);
	}

	pv.SetVolume(volume);

	if (mixer != nullptr) {
		/* update the hardware mixer volume */

		unsigned _volume = (volume * base) / PCM_VOLUME_1;
		if (_volume > 100)
			_volume = 100;

		Error error;
		if (!mixer_set_volume(mixer, _volume, error))
			LogError(error, "Failed to update hardware mixer");
	}
}

static Filter *
replay_gain_filter_init(gcc_unused const config_param &param,
			gcc_unused Error &error)
{
	return new ReplayGainFilter();
}

AudioFormat
ReplayGainFilter::Open(AudioFormat &af, gcc_unused Error &error)
{
	if (!pv.Open(af.format, error))
		return AudioFormat::Undefined();

	return af;
}

void
ReplayGainFilter::Close()
{
	pv.Close();
}

ConstBuffer<void>
ReplayGainFilter::FilterPCM(ConstBuffer<void> src, gcc_unused Error &error)
{
	return pv.Apply(src);
}

const struct filter_plugin replay_gain_filter_plugin = {
	"replay_gain",
	replay_gain_filter_init,
};

void
replay_gain_filter_set_mixer(Filter *_filter, Mixer *mixer,
			     unsigned base)
{
	ReplayGainFilter *filter = (ReplayGainFilter *)_filter;

	filter->SetMixer(mixer, base);
}

void
replay_gain_filter_set_info(Filter *_filter, const ReplayGainInfo *info)
{
	ReplayGainFilter *filter = (ReplayGainFilter *)_filter;

	filter->SetInfo(info);
}

void
replay_gain_filter_set_mode(Filter *_filter, ReplayGainMode mode)
{
	ReplayGainFilter *filter = (ReplayGainFilter *)_filter;

	filter->SetMode(mode);
}
