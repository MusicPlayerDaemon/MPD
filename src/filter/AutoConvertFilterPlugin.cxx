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
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "audio_format.h"

#include <assert.h>

class AutoConvertFilter final : public Filter {
	/**
	 * The audio format being fed to the underlying filter.  This
	 * plugin actually doesn't need this variable, we have it here
	 * just so our open() method doesn't return a stack pointer.
	 */
	audio_format child_audio_format;

	/**
	 * The underlying filter.
	 */
	Filter *filter;

	/**
	 * A convert_filter, just in case conversion is needed.  nullptr
	 * if unused.
	 */
	Filter *convert;

public:
	AutoConvertFilter(Filter *_filter):filter(_filter) {}
	~AutoConvertFilter() {
		delete filter;
	}

	virtual const audio_format *Open(audio_format &af, GError **error_r);
	virtual void Close();
	virtual const void *FilterPCM(const void *src, size_t src_size,
				      size_t *dest_size_r, GError **error_r);
};

const struct audio_format *
AutoConvertFilter::Open(audio_format &in_audio_format, GError **error_r)
{
	assert(audio_format_valid(&in_audio_format));

	/* open the "real" filter */

	child_audio_format = in_audio_format;
	const audio_format *out_audio_format =
		filter->Open(child_audio_format, error_r);
	if (out_audio_format == nullptr)
		return nullptr;

	/* need to convert? */

	if (!audio_format_equals(&child_audio_format, &in_audio_format)) {
		/* yes - create a convert_filter */

		convert = filter_new(&convert_filter_plugin, nullptr, error_r);
		if (convert == nullptr) {
			filter->Close();
			return nullptr;
		}

		audio_format audio_format2 = in_audio_format;
		const audio_format *audio_format3 =
			convert->Open(audio_format2, error_r);
		if (audio_format3 == nullptr) {
			delete convert;
			filter->Close();
			return nullptr;
		}

		assert(audio_format_equals(&audio_format2, &in_audio_format));

		convert_filter_set(convert, child_audio_format);
	} else
		/* no */
		convert = nullptr;

	return out_audio_format;
}

void
AutoConvertFilter::Close()
{
	if (convert != nullptr) {
		convert->Close();
		delete convert;
	}

	filter->Close();
}

const void *
AutoConvertFilter::FilterPCM(const void *src, size_t src_size,
			     size_t *dest_size_r, GError **error_r)
{
	if (convert != nullptr) {
		src = convert->FilterPCM(src, src_size, &src_size, error_r);
		if (src == nullptr)
			return nullptr;
	}

	return filter->FilterPCM(src, src_size, dest_size_r, error_r);
}

Filter *
autoconvert_filter_new(Filter *filter)
{
	return new AutoConvertFilter(filter);
}
