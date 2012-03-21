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
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"
#include "pcm_buffer.h"
#include "audio_format.h"
#include "AudioCompress/compress.h"

#include <assert.h>
#include <string.h>

struct normalize_filter {
	struct filter filter;

	struct Compressor *compressor;

	struct pcm_buffer buffer;
};

static inline GQuark
normalize_quark(void)
{
	return g_quark_from_static_string("normalize");
}

static struct filter *
normalize_filter_init(G_GNUC_UNUSED const struct config_param *param,
		      G_GNUC_UNUSED GError **error_r)
{
	struct normalize_filter *filter = g_new(struct normalize_filter, 1);

	filter_init(&filter->filter, &normalize_filter_plugin);

	return &filter->filter;
}

static void
normalize_filter_finish(struct filter *filter)
{
	g_free(filter);
}

static const struct audio_format *
normalize_filter_open(struct filter *_filter,
		      struct audio_format *audio_format,
		      G_GNUC_UNUSED GError **error_r)
{
	struct normalize_filter *filter = (struct normalize_filter *)_filter;

	audio_format->format = SAMPLE_FORMAT_S16;

	filter->compressor = Compressor_new(0);

	pcm_buffer_init(&filter->buffer);

	return audio_format;
}

static void
normalize_filter_close(struct filter *_filter)
{
	struct normalize_filter *filter = (struct normalize_filter *)_filter;

	pcm_buffer_deinit(&filter->buffer);
	Compressor_delete(filter->compressor);
}

static const void *
normalize_filter_filter(struct filter *_filter,
			const void *src, size_t src_size, size_t *dest_size_r,
			G_GNUC_UNUSED GError **error_r)
{
	struct normalize_filter *filter = (struct normalize_filter *)_filter;
	void *dest;

	dest = pcm_buffer_get(&filter->buffer, src_size);

	memcpy(dest, src, src_size);

	Compressor_Process_int16(filter->compressor, dest, src_size / 2);

	*dest_size_r = src_size;
	return dest;
}

const struct filter_plugin normalize_filter_plugin = {
	.name = "normalize",
	.init = normalize_filter_init,
	.finish = normalize_filter_finish,
	.open = normalize_filter_open,
	.close = normalize_filter_close,
	.filter = normalize_filter_filter,
};
