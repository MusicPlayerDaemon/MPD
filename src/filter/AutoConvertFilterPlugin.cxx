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
#include "AutoConvertFilterPlugin.hxx"
#include "ConvertFilterPlugin.hxx"
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"
#include "audio_format.h"

#include <assert.h>

struct AutoConvertFilter {
	struct filter base;

	/**
	 * The audio format being fed to the underlying filter.  This
	 * plugin actually doesn't need this variable, we have it here
	 * just so our open() method doesn't return a stack pointer.
	 */
	struct audio_format in_audio_format;

	/**
	 * The underlying filter.
	 */
	struct filter *filter;

	/**
	 * A convert_filter, just in case conversion is needed.  nullptr
	 * if unused.
	 */
	struct filter *convert;

	AutoConvertFilter(const filter_plugin &plugin, struct filter *_filter)
		:filter(_filter) {
		filter_init(&base, &plugin);
	}

	~AutoConvertFilter() {
		filter_free(filter);
	}
};

static void
autoconvert_filter_finish(struct filter *_filter)
{
	AutoConvertFilter *filter = (AutoConvertFilter *)_filter;

	delete filter;
}

static const struct audio_format *
autoconvert_filter_open(struct filter *_filter,
			struct audio_format *in_audio_format,
			GError **error_r)
{
	AutoConvertFilter *filter = (AutoConvertFilter *)_filter;
	const struct audio_format *out_audio_format;

	assert(audio_format_valid(in_audio_format));

	/* open the "real" filter */

	filter->in_audio_format = *in_audio_format;

	out_audio_format = filter_open(filter->filter,
				       &filter->in_audio_format, error_r);
	if (out_audio_format == nullptr)
		return nullptr;

	/* need to convert? */

	if (!audio_format_equals(&filter->in_audio_format, in_audio_format)) {
		/* yes - create a convert_filter */
		struct audio_format audio_format2 = *in_audio_format;
		const struct audio_format *audio_format3;

		filter->convert = filter_new(&convert_filter_plugin, nullptr,
					     error_r);
		if (filter->convert == nullptr) {
			filter_close(filter->filter);
			return nullptr;
		}

		audio_format3 = filter_open(filter->convert, &audio_format2,
					    error_r);
		if (audio_format3 == nullptr) {
			filter_free(filter->convert);
			filter_close(filter->filter);
			return nullptr;
		}

		assert(audio_format_equals(&audio_format2, in_audio_format));

		convert_filter_set(filter->convert, &filter->in_audio_format);
	} else
		/* no */
		filter->convert = nullptr;

	return out_audio_format;
}

static void
autoconvert_filter_close(struct filter *_filter)
{
	AutoConvertFilter *filter =
		(AutoConvertFilter *)_filter;

	if (filter->convert != nullptr) {
		filter_close(filter->convert);
		filter_free(filter->convert);
	}

	filter_close(filter->filter);
}

static const void *
autoconvert_filter_filter(struct filter *_filter, const void *src,
			  size_t src_size, size_t *dest_size_r,
			  GError **error_r)
{
	AutoConvertFilter *filter = (AutoConvertFilter *)_filter;

	if (filter->convert != nullptr) {
		src = filter_filter(filter->convert, src, src_size, &src_size,
				    error_r);
		if (src == nullptr)
			return nullptr;
	}

	return filter_filter(filter->filter, src, src_size, dest_size_r,
			     error_r);
}

static const struct filter_plugin autoconvert_filter_plugin = {
	"convert",
	nullptr,
	autoconvert_filter_finish,
	autoconvert_filter_open,
	autoconvert_filter_close,
	autoconvert_filter_filter,
};

struct filter *
autoconvert_filter_new(struct filter *_filter)
{
	AutoConvertFilter *filter =
		new AutoConvertFilter(autoconvert_filter_plugin,
				      _filter);

	return &filter->base;
}
