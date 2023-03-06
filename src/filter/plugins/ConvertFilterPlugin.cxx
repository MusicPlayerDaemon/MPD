// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ConvertFilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Convert.hxx"

#include <cassert>
#include <memory>

class ConvertFilter final : public Filter {
	/**
	 * The input audio format; PCM data is passed to the filter()
	 * method in this format.
	 */
	const AudioFormat in_audio_format;

	/**
	 * This object is only "open" if #in_audio_format !=
	 * #out_audio_format.
	 */
	std::unique_ptr<PcmConvert> state;

public:
	explicit ConvertFilter(const AudioFormat &audio_format);

	void Set(const AudioFormat &_out_audio_format);

	void Reset() noexcept override {
		if (state)
			state->Reset();
	}

	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override;

	std::span<const std::byte> Flush() override {
		return state
			? state->Flush()
			: std::span<const std::byte>{};
	}
};

class PreparedConvertFilter final : public PreparedFilter {
public:
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

void
ConvertFilter::Set(const AudioFormat &_out_audio_format)
{
	assert(_out_audio_format.IsValid());

	if (_out_audio_format == out_audio_format)
		/* no change */
		return;

	if (state) {
		out_audio_format = in_audio_format;
		state.reset();
	}

	if (_out_audio_format == in_audio_format)
		/* optimized special case: no-op */
		return;

	state = std::make_unique<PcmConvert>(in_audio_format,
					     _out_audio_format);

	out_audio_format = _out_audio_format;
}

ConvertFilter::ConvertFilter(const AudioFormat &audio_format)
	:Filter(audio_format), in_audio_format(audio_format)
{
	assert(in_audio_format.IsValid());
}

std::unique_ptr<Filter>
PreparedConvertFilter::Open(AudioFormat &audio_format)
{
	assert(audio_format.IsValid());

	return std::make_unique<ConvertFilter>(audio_format);
}

std::span<const std::byte>
ConvertFilter::FilterPCM(std::span<const std::byte> src)
{
	return state
		? state->Convert(src)
		/* optimized special case: no-op */
		: std::span<const std::byte>{src};
}

std::unique_ptr<PreparedFilter>
convert_filter_prepare() noexcept
{
	return std::make_unique<PreparedConvertFilter>();
}

std::unique_ptr<Filter>
convert_filter_new(const AudioFormat in_audio_format,
		   const AudioFormat out_audio_format)
{
	auto filter = std::make_unique<ConvertFilter>(in_audio_format);
	filter->Set(out_audio_format);
	return filter;
}

void
convert_filter_set(Filter *_filter, AudioFormat out_audio_format)
{
	auto *filter = (ConvertFilter *)_filter;

	filter->Set(out_audio_format);
}
