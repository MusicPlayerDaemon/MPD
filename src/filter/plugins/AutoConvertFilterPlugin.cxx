/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/FilterRegistry.hxx"
#include "AudioFormat.hxx"
#include "util/ConstBuffer.hxx"

#include <assert.h>

class AutoConvertFilter final : public Filter {
	/**
	 * The underlying filter.
	 */
	Filter *const filter;

	/**
	 * A convert_filter, just in case conversion is needed.  nullptr
	 * if unused.
	 */
	Filter *const convert;

public:
	AutoConvertFilter(Filter *_filter, Filter *_convert)
		:filter(_filter), convert(_convert) {}

	~AutoConvertFilter() {
		delete convert;
		delete filter;
	}

	virtual ConstBuffer<void> FilterPCM(ConstBuffer<void> src,
					    Error &error) override;
};

class PreparedAutoConvertFilter final : public PreparedFilter {
	/**
	 * The underlying filter.
	 */
	PreparedFilter *const filter;

public:
	PreparedAutoConvertFilter(PreparedFilter *_filter):filter(_filter) {}
	~PreparedAutoConvertFilter() {
		delete filter;
	}

	virtual Filter *Open(AudioFormat &af, Error &error) override;
};

Filter *
PreparedAutoConvertFilter::Open(AudioFormat &in_audio_format, Error &error)
{
	assert(in_audio_format.IsValid());

	/* open the "real" filter */

	AudioFormat child_audio_format = in_audio_format;
	auto *new_filter = filter->Open(child_audio_format, error);
	if (new_filter == nullptr)
		return nullptr;

	/* need to convert? */

	Filter *convert = nullptr;
	if (in_audio_format != child_audio_format) {
		/* yes - create a convert_filter */

		convert = convert_filter_new(in_audio_format,
					     child_audio_format,
					     error);
		if (convert == nullptr) {
			delete new_filter;
			return nullptr;
		}
	}

	return new AutoConvertFilter(new_filter, convert);
}

ConstBuffer<void>
AutoConvertFilter::FilterPCM(ConstBuffer<void> src, Error &error)
{
	if (convert != nullptr) {
		src = convert->FilterPCM(src, error);
		if (src.IsNull())
			return nullptr;
	}

	return filter->FilterPCM(src, error);
}

PreparedFilter *
autoconvert_filter_new(PreparedFilter *filter)
{
	return new PreparedAutoConvertFilter(filter);
}
