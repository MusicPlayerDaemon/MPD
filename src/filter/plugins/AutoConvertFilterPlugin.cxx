/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "TwoFilters.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/AudioFormat.hxx"

#include <cassert>
#include <memory>

class PreparedAutoConvertFilter final : public PreparedFilter {
	/**
	 * The underlying filter.
	 */
	std::unique_ptr<PreparedFilter> filter;

public:
	explicit PreparedAutoConvertFilter(std::unique_ptr<PreparedFilter> _filter) noexcept
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

	if (in_audio_format == child_audio_format)
		/* no */
		return new_filter;

	/* yes - create a convert_filter */

	auto convert = convert_filter_new(in_audio_format,
					  child_audio_format);

	return std::make_unique<TwoFilters>(std::move(convert),
					    std::move(new_filter));
}

std::unique_ptr<PreparedFilter>
autoconvert_filter_new(std::unique_ptr<PreparedFilter> filter) noexcept
{
	return std::make_unique<PreparedAutoConvertFilter>(std::move(filter));
}
