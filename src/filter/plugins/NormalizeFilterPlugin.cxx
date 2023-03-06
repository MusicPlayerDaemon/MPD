// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "NormalizeFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/Buffer.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/AudioCompress/compress.h"

#include <string.h>

class NormalizeFilter final : public Filter {
	Compressor *const compressor;

	PcmBuffer buffer;

public:
	explicit NormalizeFilter(const AudioFormat &audio_format)
		:Filter(audio_format), compressor(Compressor_new(0)) {
	}

	~NormalizeFilter() override {
		Compressor_delete(compressor);
	}


	NormalizeFilter(const NormalizeFilter &) = delete;
	NormalizeFilter &operator=(const NormalizeFilter &) = delete;

	/* virtual methods from class Filter */
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
NormalizeFilter::FilterPCM(std::span<const std::byte> src)
{
	auto *dest = (int16_t *)buffer.Get(src.size());
	memcpy(dest, src.data(), src.size());

	Compressor_Process_int16(compressor, dest, src.size() / 2);
	return { (const std::byte *)dest, src.size() };
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
