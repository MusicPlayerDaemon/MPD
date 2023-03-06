// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VolumeFilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/Volume.hxx"
#include "pcm/AudioFormat.hxx"

class VolumeFilter final : public Filter {
	PcmVolume pv;

public:
	explicit VolumeFilter(const AudioFormat &audio_format)
		:Filter(audio_format) {
		out_audio_format.format = pv.Open(out_audio_format.format,
						  true);
	}

	[[nodiscard]] unsigned GetVolume() const noexcept {
		return pv.GetVolume();
	}

	void SetVolume(unsigned _volume) noexcept {
		pv.SetVolume(_volume);
	}

	/* virtual methods from class Filter */
	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override;
};

class PreparedVolumeFilter final : public PreparedFilter {
public:
	/* virtual methods from class Filter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

std::unique_ptr<Filter>
PreparedVolumeFilter::Open(AudioFormat &audio_format)
{
	return std::make_unique<VolumeFilter>(audio_format);
}

std::span<const std::byte>
VolumeFilter::FilterPCM(std::span<const std::byte> src)
{
	return pv.Apply(src);
}

std::unique_ptr<PreparedFilter>
volume_filter_prepare() noexcept
{
	return std::make_unique<PreparedVolumeFilter>();
}

unsigned
volume_filter_get(const Filter *_filter) noexcept
{
	const auto *filter =
		(const VolumeFilter *)_filter;

	return filter->GetVolume();
}

void
volume_filter_set(Filter *_filter, unsigned volume) noexcept
{
	auto *filter = (VolumeFilter *)_filter;

	filter->SetVolume(volume);
}
