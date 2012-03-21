/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "filter/replay_gain_filter_plugin.h"
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"
#include "audio_format.h"
#include "pcm_buffer.h"
#include "pcm_volume.h"
#include "replay_gain_info.h"
#include "replay_gain_config.h"
#include "mixer_control.h"

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "replay_gain"

struct replay_gain_filter {
	struct filter filter;

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

	struct audio_format audio_format;

	struct pcm_buffer buffer;
};

static inline GQuark
replay_gain_quark(void)
{
	return g_quark_from_static_string("replay_gain");
}

/**
 * Recalculates the new volume after a property was changed.
 */
static void
replay_gain_filter_update(struct replay_gain_filter *filter)
{
	if (filter->mode != REPLAY_GAIN_OFF) {
		float scale = replay_gain_tuple_scale(&filter->info.tuples[filter->mode],
		    replay_gain_preamp, replay_gain_missing_preamp, replay_gain_limit);
		g_debug("scale=%f\n", (double)scale);

		filter->volume = pcm_float_to_volume(scale);
	} else
		filter->volume = PCM_VOLUME_1;

	if (filter->mixer != NULL) {
		/* update the hardware mixer volume */

		unsigned volume = (filter->volume * filter->base) / PCM_VOLUME_1;
		if (volume > 100)
			volume = 100;

		GError *error = NULL;
		if (!mixer_set_volume(filter->mixer, volume, &error)) {
			g_warning("Failed to update hardware mixer: %s",
				  error->message);
			g_error_free(error);
		}
	}
}

static struct filter *
replay_gain_filter_init(G_GNUC_UNUSED const struct config_param *param,
			G_GNUC_UNUSED GError **error_r)
{
	struct replay_gain_filter *filter = g_new(struct replay_gain_filter, 1);

	filter_init(&filter->filter, &replay_gain_filter_plugin);
	filter->mixer = NULL;

	filter->mode = replay_gain_get_real_mode();
	replay_gain_info_init(&filter->info);
	filter->volume = PCM_VOLUME_1;

	return &filter->filter;
}

static void
replay_gain_filter_finish(struct filter *filter)
{
	g_free(filter);
}

static const struct audio_format *
replay_gain_filter_open(struct filter *_filter,
			struct audio_format *audio_format,
			G_GNUC_UNUSED GError **error_r)
{
	struct replay_gain_filter *filter =
		(struct replay_gain_filter *)_filter;

	filter->audio_format = *audio_format;
	pcm_buffer_init(&filter->buffer);

	return &filter->audio_format;
}

static void
replay_gain_filter_close(struct filter *_filter)
{
	struct replay_gain_filter *filter =
		(struct replay_gain_filter *)_filter;

	pcm_buffer_deinit(&filter->buffer);
}

static const void *
replay_gain_filter_filter(struct filter *_filter,
			  const void *src, size_t src_size,
			  size_t *dest_size_r, GError **error_r)
{
	struct replay_gain_filter *filter =
		(struct replay_gain_filter *)_filter;
	bool success;
	void *dest;
	enum replay_gain_mode rg_mode;

	/* check if the mode has been changed since the last call */
	rg_mode = replay_gain_get_real_mode();

	if (filter->mode != rg_mode) {
		g_debug("replay gain mode has changed %d->%d\n", filter->mode, rg_mode);
		filter->mode = rg_mode;
		replay_gain_filter_update(filter);
	}

	*dest_size_r = src_size;

	if (filter->volume == PCM_VOLUME_1)
		/* optimized special case: 100% volume = no-op */
		return src;

	dest = pcm_buffer_get(&filter->buffer, src_size);

	if (filter->volume <= 0) {
		/* optimized special case: 0% volume = memset(0) */
		/* XXX is this valid for all sample formats? What
		   about floating point? */
		memset(dest, 0, src_size);
		return dest;
	}

	memcpy(dest, src, src_size);

	success = pcm_volume(dest, src_size, filter->audio_format.format,
			     filter->volume);
	if (!success) {
		g_set_error(error_r, replay_gain_quark(), 0,
			    "pcm_volume() has failed");
		return NULL;
	}

	return dest;
}

const struct filter_plugin replay_gain_filter_plugin = {
	.name = "replay_gain",
	.init = replay_gain_filter_init,
	.finish = replay_gain_filter_finish,
	.open = replay_gain_filter_open,
	.close = replay_gain_filter_close,
	.filter = replay_gain_filter_filter,
};

void
replay_gain_filter_set_mixer(struct filter *_filter, struct mixer *mixer,
			     unsigned base)
{
	struct replay_gain_filter *filter =
		(struct replay_gain_filter *)_filter;

	assert(mixer == NULL || (base > 0 && base <= 100));

	filter->mixer = mixer;
	filter->base = base;

	replay_gain_filter_update(filter);
}

void
replay_gain_filter_set_info(struct filter *_filter,
			    const struct replay_gain_info *info)
{
	struct replay_gain_filter *filter =
		(struct replay_gain_filter *)_filter;

	if (info != NULL) {
		filter->info = *info;
		replay_gain_info_complete(&filter->info);
	} else
		replay_gain_info_init(&filter->info);

	replay_gain_filter_update(filter);
}
