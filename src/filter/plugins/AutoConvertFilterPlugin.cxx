/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "AutoConvertFilterPlugin.hxx"
#include "ConvertFilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "AudioFormat.hxx"
#include "util/ConstBuffer.hxx"

#include <memory>

#include <assert.h>

class AutoConvertFilter final : public Filter {
	/**
	 * The underlying filter.
	 */
	std::unique_ptr<Filter> filter;

	/**
	 * A convert_filter, just in case conversion is needed.  nullptr
	 * if unused.
	 */
	std::unique_ptr<Filter> convert;

public:
	AutoConvertFilter(std::unique_ptr<Filter> &&_filter,
			  std::unique_ptr<Filter> &&_convert)
		:Filter(_filter->GetOutAudioFormat()),
		 filter(std::move(_filter)), convert(std::move(_convert)) {}

	void Reset() noexcept override {
		filter->Reset();

		if (convert)
			convert->Reset();
	}

	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;
	ConstBuffer<void> Flush() override;
};

class PreparedAutoConvertFilter final : public PreparedFilter {
	/**
	 * The underlying filter.
	 */
	std::unique_ptr<PreparedFilter> filter;

public:
	PreparedAutoConvertFilter(std::unique_ptr<PreparedFilter> _filter) noexcept
		:filter(std::move(_filter)) {}

	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

std::unique_ptr<Filter>
PreparedAutoConvertFilter::Open(AudioFormat &in_audio_format)
{
	assert(in_audio_format.IsValid());

	/* open the "real" filter */

	AudioFormat child_audio_format = in_audio_format;
	auto new_filter = filter->Open(child_audio_format);

	/* need to convert? */

	std::unique_ptr<Filter> convert;
	if (in_audio_format != child_audio_format) {
		/* yes - create a convert_filter */

		convert.reset(convert_filter_new(in_audio_format,
						 child_audio_format));
	}

	return std::make_unique<AutoConvertFilter>(std::move(new_filter),
						   std::move(convert));
}

ConstBuffer<void>
AutoConvertFilter::FilterPCM(ConstBuffer<void> src)
{
	if (convert != nullptr)
		src = convert->FilterPCM(src);

	return filter->FilterPCM(src);
}

ConstBuffer<void>
AutoConvertFilter::Flush()
{
	if (convert != nullptr) {
		auto result = convert->Flush();
		if (!result.IsNull())
			return filter->FilterPCM(result);
	}

	return filter->Flush();
}

std::unique_ptr<PreparedFilter>
autoconvert_filter_new(std::unique_ptr<PreparedFilter> filter) noexcept
{
	return std::make_unique<PreparedAutoConvertFilter>(std::move(filter));
}
