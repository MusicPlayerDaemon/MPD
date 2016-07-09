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
#include "VolumeFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/FilterRegistry.hxx"
#include "pcm/Volume.hxx"
#include "AudioFormat.hxx"
#include "util/ConstBuffer.hxx"

class VolumeFilter final : public Filter {
	PcmVolume pv;

public:
	explicit VolumeFilter(const AudioFormat &audio_format)
		:Filter(audio_format) {}

	bool Open(Error &error) {
		return pv.Open(out_audio_format.format, error);
	}

	unsigned GetVolume() const {
		return pv.GetVolume();
	}

	void SetVolume(unsigned _volume) {
		pv.SetVolume(_volume);
	}

	/* virtual methods from class Filter */
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src,
				    Error &error) override;
};

class PreparedVolumeFilter final : public PreparedFilter {
	PcmVolume pv;

public:
	/* virtual methods from class Filter */
	Filter *Open(AudioFormat &af, Error &error) override;
};

static PreparedFilter *
volume_filter_init(gcc_unused const ConfigBlock &block,
		   gcc_unused Error &error)
{
	return new PreparedVolumeFilter();
}

Filter *
PreparedVolumeFilter::Open(AudioFormat &audio_format, Error &error)
{
	auto *filter = new VolumeFilter(audio_format);
	if (!filter->Open(error)) {
		delete filter;
		return nullptr;
	}

	return filter;
}

ConstBuffer<void>
VolumeFilter::FilterPCM(ConstBuffer<void> src, gcc_unused Error &error)
{
	return pv.Apply(src);
}

const struct filter_plugin volume_filter_plugin = {
	"volume",
	volume_filter_init,
};

unsigned
volume_filter_get(const Filter *_filter)
{
	const VolumeFilter *filter =
		(const VolumeFilter *)_filter;

	return filter->GetVolume();
}

void
volume_filter_set(Filter *_filter, unsigned volume)
{
	VolumeFilter *filter = (VolumeFilter *)_filter;

	filter->SetVolume(volume);
}

