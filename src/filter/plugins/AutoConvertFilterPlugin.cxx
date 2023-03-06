// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
