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
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "conf.h"
#include "PcmConvert.hxx"
#include "util/Manual.hxx"
#include "audio_format.h"
#include "poison.h"

#include <assert.h>
#include <string.h>

class ConvertFilter final : public Filter {
	/**
	 * The input audio format; PCM data is passed to the filter()
	 * method in this format.
	 */
	audio_format in_audio_format;

	/**
	 * The output audio format; the consumer of this plugin
	 * expects PCM data in this format.  This defaults to
	 * #in_audio_format, and can be set with convert_filter_set().
	 */
	audio_format out_audio_format;

	Manual<PcmConvert> state;

public:
	void Set(const audio_format &_out_audio_format) {
		assert(audio_format_valid(&in_audio_format));
		assert(audio_format_valid(&out_audio_format));
		assert(audio_format_valid(&_out_audio_format));

		out_audio_format = _out_audio_format;
	}

	virtual const audio_format *Open(audio_format &af, GError **error_r);
	virtual void Close();
	virtual const void *FilterPCM(const void *src, size_t src_size,
				      size_t *dest_size_r, GError **error_r);
};

static Filter *
convert_filter_init(gcc_unused const struct config_param *param,
		    gcc_unused GError **error_r)
{
	return new ConvertFilter();
}

const struct audio_format *
ConvertFilter::Open(audio_format &audio_format, gcc_unused GError **error_r)
{
	assert(audio_format_valid(&audio_format));

	in_audio_format = out_audio_format = audio_format;
	state.Construct();

	return &in_audio_format;
}

void
ConvertFilter::Close()
{
	state.Destruct();

	poison_undefined(&in_audio_format, sizeof(in_audio_format));
	poison_undefined(&out_audio_format, sizeof(out_audio_format));
}

const void *
ConvertFilter::FilterPCM(const void *src, size_t src_size,
			 size_t *dest_size_r, GError **error_r)
{
	if (audio_format_equals(&in_audio_format, &out_audio_format)) {
		/* optimized special case: no-op */
		*dest_size_r = src_size;
		return src;
	}

	return state->Convert(&in_audio_format,
			      src, src_size,
			      &out_audio_format, dest_size_r,
			      error_r);
}

const struct filter_plugin convert_filter_plugin = {
	"convert",
	convert_filter_init,
};

void
convert_filter_set(Filter *_filter, const audio_format &out_audio_format)
{
	ConvertFilter *filter = (ConvertFilter *)_filter;

	filter->Set(out_audio_format);
}
