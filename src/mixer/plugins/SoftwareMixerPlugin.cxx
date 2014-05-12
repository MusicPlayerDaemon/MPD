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
#include "SoftwareMixerPlugin.hxx"
#include "mixer/MixerInternal.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterRegistry.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/plugins/VolumeFilterPlugin.hxx"
#include "pcm/Volume.hxx"
#include "config/ConfigData.hxx"
#include "util/Error.hxx"

#include <assert.h>
#include <math.h>

static Filter *
CreateVolumeFilter()
{
	return filter_new(&volume_filter_plugin, config_param(),
			  IgnoreError());
}

class SoftwareMixer final : public Mixer {
	Filter *filter;

	/**
	 * If this is true, then this object "owns" the #Filter
	 * instance (see above).  It will be set to false by
	 * software_mixer_get_filter(); after that, the caller will be
	 * responsible for the #Filter.
	 */
	bool owns_filter;

	/**
	 * The current volume in percent (0..100).
	 */
	unsigned volume;

public:
	SoftwareMixer(MixerListener &_listener)
		:Mixer(software_mixer_plugin, _listener),
		 filter(CreateVolumeFilter()),
		 owns_filter(true),
		 volume(100)
	{
		assert(filter != nullptr);
	}

	virtual ~SoftwareMixer() {
		if (owns_filter)
			delete filter;
	}

	Filter *GetFilter();

	/* virtual methods from class Mixer */
	virtual bool Open(gcc_unused Error &error) override {
		return true;
	}

	virtual void Close() override {
	}

	virtual int GetVolume(gcc_unused Error &error) override {
		return volume;
	}

	virtual bool SetVolume(unsigned volume, Error &error) override;
};

static Mixer *
software_mixer_init(gcc_unused EventLoop &event_loop,
		    gcc_unused AudioOutput &ao,
		    MixerListener &listener,
		    gcc_unused const config_param &param,
		    gcc_unused Error &error)
{
	return new SoftwareMixer(listener);
}

gcc_const
static unsigned
PercentVolumeToSoftwareVolume(unsigned volume)
{
	assert(volume <= 100);

	if (volume >= 100)
		return PCM_VOLUME_1;
	else if (volume > 0)
		return pcm_float_to_volume((exp(volume / 25.0) - 1) /
					   (54.5981500331F - 1));
	else
		return 0;
}

bool
SoftwareMixer::SetVolume(unsigned new_volume, gcc_unused Error &error)
{
	assert(new_volume <= 100);

	volume = new_volume;
	volume_filter_set(filter, PercentVolumeToSoftwareVolume(new_volume));
	return true;
}

const MixerPlugin software_mixer_plugin = {
	software_mixer_init,
	true,
};

inline Filter *
SoftwareMixer::GetFilter()
{
	assert(owns_filter);

	owns_filter = false;
	return filter;
}

Filter *
software_mixer_get_filter(Mixer *mixer)
{
	SoftwareMixer *sm = (SoftwareMixer *)mixer;
	assert(sm->IsPlugin(software_mixer_plugin));
	return sm->GetFilter();
}
