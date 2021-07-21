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

#include "VolumeFilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/Volume.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/ConstBuffer.hxx"

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
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;
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

ConstBuffer<void>
VolumeFilter::FilterPCM(ConstBuffer<void> src)
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
