// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TwoFilters.hxx"
#include "pcm/AudioFormat.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringBuffer.hxx"

std::span<const std::byte>
TwoFilters::FilterPCM(std::span<const std::byte> src)
{
	if (const auto dest = first->FilterPCM(src); dest.empty()) [[unlikely]]
		/* no output from the first filter; pass the empty
                   buffer on, do not call the second filter */
		return dest;
	else
		/* pass output from the first filter to the second
                   filter and return its result */
		return second->FilterPCM(dest);
}

std::span<const std::byte>
TwoFilters::ReadMore()
{
	assert(first);
	assert(second);

	/* first read all remaining data from the second filter */
	if (auto result = second->ReadMore(); !result.empty())
		return result;

	/* now read more data from the first filter and process it
           with the second filter */
	if (auto result = first->ReadMore(); !result.empty())
		/* output from the first Filter must be filtered by
		   the second Filter */
		return second->FilterPCM(result);

	/* both filters have been queried and there's no more data */
	return {};
}

std::span<const std::byte>
TwoFilters::Flush()
{
	assert(second);

	/* first read all remaining data from the second filter */
	if (auto result = second->ReadMore(); !result.empty())
		return result;

	/* now flush the first filter and process it with the second
           filter */
	if (first) {
		if (auto result = first->Flush(); !result.empty())
			/* output from the first Filter must be
			   filtered by the second Filter */
			return second->FilterPCM(result);

		first.reset();
	}

	/* finally flush the second filter */
	return second->Flush();
}

std::unique_ptr<Filter>
PreparedTwoFilters::Open(AudioFormat &audio_format)
{
	auto a = first->Open(audio_format);

	const auto &a_out_format = a->GetOutAudioFormat();
	auto b_in_format = a_out_format;
	auto b = second->Open(b_in_format);

	if (b_in_format != a_out_format)
		throw FmtRuntimeError("Audio format not supported by filter {:?}: {}",
				      second_name, a_out_format);

	return std::make_unique<TwoFilters>(std::move(a),
					    std::move(b));
}
