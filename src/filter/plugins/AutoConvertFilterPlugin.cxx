/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "config/ConfigData.hxx"
#include "util/ConstBuffer.hxx"

#include <assert.h>

class AutoConvertFilter final : public Filter {
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

	virtual AudioFormat Open(AudioFormat &af, Error &error) override;
	virtual void Close() override;
	virtual ConstBuffer<void> FilterPCM(ConstBuffer<void> src,
					    Error &error) override;
};

AudioFormat
AutoConvertFilter::Open(AudioFormat &in_audio_format, Error &error)
{
	assert(in_audio_format.IsValid());

	/* open the "real" filter */

	AudioFormat child_audio_format = in_audio_format;
	AudioFormat out_audio_format = filter->Open(child_audio_format, error);
	if (!out_audio_format.IsDefined())
		return out_audio_format;

	/* need to convert? */

	if (in_audio_format != child_audio_format) {
		/* yes - create a convert_filter */

		const config_param empty;
		convert = filter_new(&convert_filter_plugin, empty, error);
		if (convert == nullptr) {
			filter->Close();
			return AudioFormat::Undefined();
		}

		AudioFormat audio_format2 = in_audio_format;
		AudioFormat audio_format3 =
			convert->Open(audio_format2, error);
		if (!audio_format3.IsDefined()) {
			delete convert;
			filter->Close();
			return AudioFormat::Undefined();
		}

		assert(audio_format2 == in_audio_format);

		if (!convert_filter_set(convert, child_audio_format, error)) {
			delete convert;
			filter->Close();
			return AudioFormat::Undefined();
		}
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

Filter *
autoconvert_filter_new(Filter *filter)
{
	return new AutoConvertFilter(filter);
}
