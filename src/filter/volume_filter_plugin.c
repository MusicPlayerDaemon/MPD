/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "filter/volume_filter_plugin.h"
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"
#include "conf.h"
#include "pcm_buffer.h"
#include "pcm_volume.h"
#include "audio_format.h"
#include "player_control.h"

#include <assert.h>
#include <string.h>

struct volume_filter {
	struct filter filter;

	/**
	 * The current volume, from 0 to #PCM_VOLUME_1.
	 */
	unsigned volume;

	struct audio_format audio_format;

	struct pcm_buffer buffer;
};

static inline GQuark
volume_quark(void)
{
	return g_quark_from_static_string("pcm_volume");
}

static struct filter *
volume_filter_init(G_GNUC_UNUSED const struct config_param *param,
		   G_GNUC_UNUSED GError **error_r)
{
	struct volume_filter *filter = g_new(struct volume_filter, 1);

	filter_init(&filter->filter, &volume_filter_plugin);
	filter->volume = PCM_VOLUME_1;

	return &filter->filter;
}

static void
volume_filter_finish(struct filter *filter)
{
	g_free(filter);
}

static const struct audio_format *
volume_filter_open(struct filter *_filter,
		   const struct audio_format *audio_format,
		   GError **error_r)
{
	struct volume_filter *filter = (struct volume_filter *)_filter;

	if (audio_format->bits != 8 && audio_format->bits != 16 &&
	    audio_format->bits != 24) {
		g_set_error(error_r, volume_quark(), 0,
			    "Unsupported audio format");
		return false;
	}

	if (audio_format->reverse_endian) {
		g_set_error(error_r, volume_quark(), 0,
			    "Software volume for reverse endian "
			    "samples is not implemented");
		return false;
	}

	filter->audio_format = *audio_format;
	pcm_buffer_init(&filter->buffer);

	return &filter->audio_format;
}

static void
volume_filter_close(struct filter *_filter)
{
	struct volume_filter *filter = (struct volume_filter *)_filter;

	pcm_buffer_deinit(&filter->buffer);
}

static const void *
volume_filter_filter(struct filter *_filter, const void *src, size_t src_size,
		     size_t *dest_size_r, GError **error_r)
{
	struct volume_filter *filter = (struct volume_filter *)_filter;
	bool success;
	void *dest;

	if (filter->volume >= PCM_VOLUME_1) {
		/* optimized special case: 100% volume = no-op */
		*dest_size_r = src_size;
		return src;
	}

	dest = pcm_buffer_get(&filter->buffer, src_size);
	*dest_size_r = src_size;

	if (filter->volume <= 0) {
		/* optimized special case: 0% volume = memset(0) */
		/* XXX is this valid for all sample formats? What
		   about floating point? */
		memset(dest, 0, src_size);
		return dest;
	}

	memcpy(dest, src, src_size);

	success = pcm_volume(dest, src_size, &filter->audio_format,
			     filter->volume);
	if (!success) {
		g_set_error(error_r, volume_quark(), 0,
			    "pcm_volume() has failed");
		return NULL;
	}

	return dest;
}

const struct filter_plugin volume_filter_plugin = {
	.name = "volume",
	.init = volume_filter_init,
	.finish = volume_filter_finish,
	.open = volume_filter_open,
	.close = volume_filter_close,
	.filter = volume_filter_filter,
};

unsigned
volume_filter_get(const struct filter *_filter)
{
	const struct volume_filter *filter =
		(const struct volume_filter *)_filter;

	assert(filter->filter.plugin == &volume_filter_plugin);
	assert(filter->volume <= PCM_VOLUME_1);

	return filter->volume;
}

void
volume_filter_set(struct filter *_filter, unsigned volume)
{
	struct volume_filter *filter = (struct volume_filter *)_filter;

	assert(filter->filter.plugin == &volume_filter_plugin);
	assert(volume <= PCM_VOLUME_1);

	filter->volume = volume;
}

