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
#include "ReplayGainFilterPlugin.hxx"
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "audio_format.h"
#include "replay_gain_info.h"
#include "replay_gain_config.h"
#include "mixer_control.h"
#include "PcmVolume.hxx"

extern "C" {
#include "pcm_buffer.h"
}

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "replay_gain"

class ReplayGainFilter final : public Filter {
	/**
	 * If set, then this hardware mixer is used for applying
	 * replay gain, instead of the software volume library.
	 */
	struct mixer *mixer;

	/**
	 * The base volume level for scale=1.0, between 1 and 100
	 * (including).
	 */
	unsigned base;

	enum replay_gain_mode mode;

	struct replay_gain_info info;

	/**
	 * The current volume, between 0 and a value that may or may not exceed
	 * #PCM_VOLUME_1.
	 *
	 * If the default value of true is used for replaygain_limit, the
	 * application of the volume to the signal will never cause clipping.
	 *
	 * On the other hand, if the user has set replaygain_limit to false,
	 * the chance of clipping is explicitly preferred if that's required to
	 * maintain a consistent audio level. Whether clipping will actually
	 * occur depends on what value the user is using for replaygain_preamp.
	 */
	unsigned volume;

	struct audio_format format;

	struct pcm_buffer buffer;

public:
	ReplayGainFilter()
		:mixer(nullptr), mode(REPLAY_GAIN_OFF),
		volume(PCM_VOLUME_1) {
		replay_gain_info_init(&info);
	}

	void SetMixer(struct mixer *_mixer, unsigned _base) {
		assert(_mixer == NULL || (_base > 0 && _base <= 100));

		mixer = _mixer;
		base = _base;

		Update();
	}

	void SetInfo(const struct replay_gain_info *_info) {
		if (_info != NULL) {
			info = *_info;
			replay_gain_info_complete(&info);
		} else
			replay_gain_info_init(&info);

		Update();
	}

	void SetMode(enum replay_gain_mode _mode) {
		if (_mode == mode)
			/* no change */
			return;

		g_debug("replay gain mode has changed %d->%d\n", mode, _mode);

		mode = _mode;
		Update();
	}

	/**
	 * Recalculates the new volume after a property was changed.
	 */
	void Update();

	virtual const audio_format *Open(audio_format &af, GError **error_r);
	virtual void Close();
	virtual const void *FilterPCM(const void *src, size_t src_size,
				      size_t *dest_size_r, GError **error_r);
};

static inline GQuark
replay_gain_quark(void)
{
	return g_quark_from_static_string("replay_gain");
}

void
ReplayGainFilter::Update()
{
	if (mode != REPLAY_GAIN_OFF) {
		float scale = replay_gain_tuple_scale(&info.tuples[mode],
		    replay_gain_preamp, replay_gain_missing_preamp, replay_gain_limit);
		g_debug("scale=%f\n", (double)scale);

		volume = pcm_float_to_volume(scale);
	} else
		volume = PCM_VOLUME_1;

	if (mixer != NULL) {
		/* update the hardware mixer volume */

		unsigned _volume = (volume * base) / PCM_VOLUME_1;
		if (_volume > 100)
			_volume = 100;

		GError *error = NULL;
		if (!mixer_set_volume(mixer, _volume, &error)) {
			g_warning("Failed to update hardware mixer: %s",
				  error->message);
			g_error_free(error);
		}
	}
}

static Filter *
replay_gain_filter_init(gcc_unused const struct config_param *param,
			gcc_unused GError **error_r)
{
	return new ReplayGainFilter();
}

const audio_format *
ReplayGainFilter::Open(audio_format &af, gcc_unused GError **error_r)
{
	format = af;
	pcm_buffer_init(&buffer);

	return &format;
}

void
ReplayGainFilter::Close()
{
	pcm_buffer_deinit(&buffer);
}

const void *
ReplayGainFilter::FilterPCM(const void *src, size_t src_size,
			    size_t *dest_size_r, GError **error_r)
{

	*dest_size_r = src_size;

	if (volume == PCM_VOLUME_1)
		/* optimized special case: 100% volume = no-op */
		return src;

	void *dest = pcm_buffer_get(&buffer, src_size);
	if (volume <= 0) {
		/* optimized special case: 0% volume = memset(0) */
		/* XXX is this valid for all sample formats? What
		   about floating point? */
		memset(dest, 0, src_size);
		return dest;
	}

	memcpy(dest, src, src_size);

	bool success = pcm_volume(dest, src_size,
				  sample_format(format.format),
				  volume);
	if (!success) {
		g_set_error(error_r, replay_gain_quark(), 0,
			    "pcm_volume() has failed");
		return NULL;
	}

	return dest;
}

const struct filter_plugin replay_gain_filter_plugin = {
	"replay_gain",
	replay_gain_filter_init,
};

void
replay_gain_filter_set_mixer(Filter *_filter, struct mixer *mixer,
			     unsigned base)
{
	ReplayGainFilter *filter = (ReplayGainFilter *)_filter;

	filter->SetMixer(mixer, base);
}

void
replay_gain_filter_set_info(Filter *_filter, const replay_gain_info *info)
{
	ReplayGainFilter *filter = (ReplayGainFilter *)_filter;

	filter->SetInfo(info);
}

void
replay_gain_filter_set_mode(Filter *_filter, enum replay_gain_mode mode)
{
	ReplayGainFilter *filter = (ReplayGainFilter *)_filter;

	filter->SetMode(mode);
}
