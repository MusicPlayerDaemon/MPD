// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "NormalizeFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/Buffer.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Normalizer.hxx"
#include "util/SpanCast.hxx"

class NormalizeFilter final : public Filter {
	PcmNormalizer normalizer;

	PcmBuffer buffer;

public:
	explicit NormalizeFilter(const AudioFormat &audio_format)
		:Filter(audio_format) {
	}

	NormalizeFilter(const NormalizeFilter &) = delete;
	NormalizeFilter &operator=(const NormalizeFilter &) = delete;

	/* virtual methods from class Filter */
	void Reset() noexcept override {
		normalizer.Reset();
	}

	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override;
};

class PreparedNormalizeFilter final : public PreparedFilter {
public:
	/* virtual methods from class PreparedFilter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

static std::unique_ptr<PreparedFilter>
normalize_filter_init([[maybe_unused]] const ConfigBlock &block)
{
	return std::make_unique<PreparedNormalizeFilter>();
}

std::unique_ptr<Filter>
PreparedNormalizeFilter::Open(AudioFormat &audio_format)
{
	audio_format.format = SampleFormat::S16;

	return std::make_unique<NormalizeFilter>(audio_format);
}

std::span<const std::byte>
NormalizeFilter::FilterPCM(std::span<const std::byte> _src)
{
	const auto src = FromBytesStrict<const int16_t>(_src);
	auto *dest = (int16_t *)buffer.GetT<int16_t>(src.size());

	normalizer.ProcessS16(dest, src);
	return std::as_bytes(std::span{dest, src.size()});
}

const FilterPlugin normalize_filter_plugin = {
	"normalize",
	normalize_filter_init,
};

std::unique_ptr<PreparedFilter>
normalize_filter_prepare() noexcept
{
	return std::make_unique<PreparedNormalizeFilter>();
}
