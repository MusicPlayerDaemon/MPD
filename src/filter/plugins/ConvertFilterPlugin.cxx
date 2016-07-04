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
#include "ConvertFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/FilterRegistry.hxx"
#include "pcm/PcmConvert.hxx"
#include "util/Manual.hxx"
#include "util/ConstBuffer.hxx"
#include "AudioFormat.hxx"
#include "poison.h"

#include <assert.h>

class ConvertFilter final : public Filter {
	/**
	 * The input audio format; PCM data is passed to the filter()
	 * method in this format.
	 */
	AudioFormat in_audio_format;

	/**
	 * This object is only "open" if #in_audio_format !=
	 * #out_audio_format.
	 */
	PcmConvert state;

public:
	ConvertFilter(const AudioFormat &audio_format);
	~ConvertFilter();

	bool Set(const AudioFormat &_out_audio_format, Error &error);

	virtual ConstBuffer<void> FilterPCM(ConstBuffer<void> src,
					    Error &error) override;
};

class PreparedConvertFilter final : public PreparedFilter {
public:
	bool Set(const AudioFormat &_out_audio_format, Error &error);

	Filter *Open(AudioFormat &af, Error &error) override;
};

static PreparedFilter *
convert_filter_init(gcc_unused const ConfigBlock &block,
		    gcc_unused Error &error)
{
	return new PreparedConvertFilter();
}

bool
ConvertFilter::Set(const AudioFormat &_out_audio_format, Error &error)
{
	assert(in_audio_format.IsValid());
	assert(_out_audio_format.IsValid());

	if (_out_audio_format == out_audio_format)
		/* no change */
		return true;

	if (out_audio_format != in_audio_format) {
		out_audio_format = in_audio_format;
		state.Close();
	}

	if (_out_audio_format == in_audio_format)
		/* optimized special case: no-op */
		return true;

	if (!state.Open(in_audio_format, _out_audio_format, error))
		return false;

	out_audio_format = _out_audio_format;
	return true;
}

ConvertFilter::ConvertFilter(const AudioFormat &audio_format)
	:Filter(audio_format), in_audio_format(audio_format)
{
}

Filter *
PreparedConvertFilter::Open(AudioFormat &audio_format, gcc_unused Error &error)
{
	assert(audio_format.IsValid());

	return new ConvertFilter(audio_format);
}

ConvertFilter::~ConvertFilter()
{
	assert(in_audio_format.IsValid());

	if (out_audio_format != in_audio_format)
		state.Close();
}

ConstBuffer<void>
ConvertFilter::FilterPCM(ConstBuffer<void> src, Error &error)
{
	assert(in_audio_format.IsValid());

	if (out_audio_format == in_audio_format)
		/* optimized special case: no-op */
		return src;

	return state.Convert(src, error);
}

const struct filter_plugin convert_filter_plugin = {
	"convert",
	convert_filter_init,
};

Filter *
convert_filter_new(const AudioFormat in_audio_format,
		   const AudioFormat out_audio_format,
		   Error &error)
{
	auto *filter = new ConvertFilter(in_audio_format);
	if (!filter->Set(out_audio_format, error)) {
		delete filter;
		return nullptr;
	}

	return filter;
}

bool
convert_filter_set(Filter *_filter, AudioFormat out_audio_format,
		   Error &error)
{
	ConvertFilter *filter = (ConvertFilter *)_filter;

	return filter->Set(out_audio_format, error);
}
