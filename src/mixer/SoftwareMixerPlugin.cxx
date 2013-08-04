/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "MixerInternal.hxx"
#include "FilterPlugin.hxx"
#include "FilterRegistry.hxx"
#include "FilterInternal.hxx"
#include "filter/VolumeFilterPlugin.hxx"
#include "pcm/PcmVolume.hxx"
#include "ConfigData.hxx"

#include <assert.h>
#include <math.h>

struct SoftwareMixer final : public Mixer {
	Filter *filter;

	unsigned volume;

	SoftwareMixer()
		:Mixer(software_mixer_plugin),
		filter(filter_new(&volume_filter_plugin, config_param(),
				  nullptr)),
		volume(100)
	{
		assert(filter != nullptr);
	}

	~SoftwareMixer() {
		delete filter;
	}
};

static Mixer *
software_mixer_init(gcc_unused void *ao,
		    gcc_unused const config_param &param,
		    gcc_unused GError **error_r)
{
	return new SoftwareMixer();
}

static void
software_mixer_finish(Mixer *data)
{
	SoftwareMixer *sm = (SoftwareMixer *)data;

	delete sm;
}

static int
software_mixer_get_volume(Mixer *mixer, gcc_unused GError **error_r)
{
	SoftwareMixer *sm = (SoftwareMixer *)mixer;

	return sm->volume;
}

static bool
software_mixer_set_volume(Mixer *mixer, unsigned volume,
			  gcc_unused GError **error_r)
{
	SoftwareMixer *sm = (SoftwareMixer *)mixer;

	assert(volume <= 100);

	sm->volume = volume;

	if (volume >= 100)
		volume = PCM_VOLUME_1;
	else if (volume > 0)
		volume = pcm_float_to_volume((exp(volume / 25.0) - 1) /
					     (54.5981500331F - 1));

	volume_filter_set(sm->filter, volume);
	return true;
}

const struct mixer_plugin software_mixer_plugin = {
	software_mixer_init,
	software_mixer_finish,
	nullptr,
	nullptr,
	software_mixer_get_volume,
	software_mixer_set_volume,
	true,
};

Filter *
software_mixer_get_filter(Mixer *mixer)
{
	SoftwareMixer *sm = (SoftwareMixer *)mixer;
	assert(sm->IsPlugin(software_mixer_plugin));

	return sm->filter;
}
