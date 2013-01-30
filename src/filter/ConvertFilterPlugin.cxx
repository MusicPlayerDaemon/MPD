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
#include "ConvertFilterPlugin.hxx"
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"
#include "conf.h"
#include "pcm_convert.h"
#include "audio_format.h"
#include "poison.h"

#include <assert.h>
#include <string.h>

struct ConvertFilter {
	struct filter base;

	/**
	 * The current convert, from 0 to #PCM_CONVERT_1.
	 */
	unsigned convert;

	/**
	 * The input audio format; PCM data is passed to the filter()
	 * method in this format.
	 */
	struct audio_format in_audio_format;

	/**
	 * The output audio format; the consumer of this plugin
	 * expects PCM data in this format.  This defaults to
	 * #in_audio_format, and can be set with convert_filter_set().
	 */
	struct audio_format out_audio_format;

	struct pcm_convert_state state;

	ConvertFilter() {
		filter_init(&base, &convert_filter_plugin);
	}
};

static struct filter *
convert_filter_init(gcc_unused const struct config_param *param,
		    gcc_unused GError **error_r)
{
	ConvertFilter *filter = new ConvertFilter();
	return &filter->base;
}

static void
convert_filter_finish(struct filter *filter)
{
	delete filter;
}

static const struct audio_format *
convert_filter_open(struct filter *_filter, struct audio_format *audio_format,
		    gcc_unused GError **error_r)
{
	ConvertFilter *filter = (ConvertFilter *)_filter;

	assert(audio_format_valid(audio_format));

	filter->in_audio_format = filter->out_audio_format = *audio_format;
	pcm_convert_init(&filter->state);

	return &filter->in_audio_format;
}

static void
convert_filter_close(struct filter *_filter)
{
	ConvertFilter *filter = (ConvertFilter *)_filter;

	pcm_convert_deinit(&filter->state);

	poison_undefined(&filter->in_audio_format,
			 sizeof(filter->in_audio_format));
	poison_undefined(&filter->out_audio_format,
			 sizeof(filter->out_audio_format));
}

static const void *
convert_filter_filter(struct filter *_filter, const void *src, size_t src_size,
		     size_t *dest_size_r, GError **error_r)
{
	ConvertFilter *filter = (ConvertFilter *)_filter;
	const void *dest;

	if (audio_format_equals(&filter->in_audio_format,
				&filter->out_audio_format)) {
		/* optimized special case: no-op */
		*dest_size_r = src_size;
		return src;
	}

	dest = pcm_convert(&filter->state, &filter->in_audio_format,
			   src, src_size,
			   &filter->out_audio_format, dest_size_r,
			   error_r);
	if (dest == NULL)
		return NULL;

	return dest;
}

const struct filter_plugin convert_filter_plugin = {
	"convert",
	convert_filter_init,
	convert_filter_finish,
	convert_filter_open,
	convert_filter_close,
	convert_filter_filter,
};

void
convert_filter_set(struct filter *_filter,
		   const struct audio_format *out_audio_format)
{
	ConvertFilter *filter = (ConvertFilter *)_filter;

	assert(filter != NULL);
	assert(audio_format_valid(&filter->in_audio_format));
	assert(audio_format_valid(&filter->out_audio_format));
	assert(out_audio_format != NULL);
	assert(audio_format_valid(out_audio_format));

	filter->out_audio_format = *out_audio_format;
}
