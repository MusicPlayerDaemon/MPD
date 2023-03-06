// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SoftwareMixerPlugin.hxx"
#include "mixer/Mixer.hxx"
#include "filter/plugins/VolumeFilterPlugin.hxx"
#include "pcm/Volume.hxx"

#include <cassert>
#include <cmath>

class SoftwareMixer final : public Mixer {
	Filter *filter = nullptr;

	/**
	 * The current volume in percent (0..100).
	 */
	unsigned volume = 100;

public:
	explicit SoftwareMixer(MixerListener &_listener)
		:Mixer(software_mixer_plugin, _listener)
	{
	}

	void SetFilter(Filter *_filter) noexcept;

	/* virtual methods from class Mixer */
	void Open() override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override {
		return volume;
	}

	void SetVolume(unsigned volume) override;
};

static Mixer *
software_mixer_init([[maybe_unused]] EventLoop &event_loop,
		    [[maybe_unused]] AudioOutput &ao,
		    MixerListener &listener,
		    [[maybe_unused]] const ConfigBlock &block)
{
	return new SoftwareMixer(listener);
}

[[gnu::const]]
static unsigned
PercentVolumeToSoftwareVolume(unsigned volume) noexcept
{
	assert(volume <= 100);

	if (volume == 100)
		return PCM_VOLUME_1;

	if (volume > 0)
		return pcm_float_to_volume((std::exp(volume / 25.0f) - 1) /
					   (54.5981500331F - 1));

	return 0;
}

void
SoftwareMixer::SetVolume(unsigned new_volume)
{
	assert(new_volume <= 100);

	volume = new_volume;

	if (filter != nullptr)
		volume_filter_set(filter, PercentVolumeToSoftwareVolume(new_volume));
}

const MixerPlugin software_mixer_plugin = {
	software_mixer_init,
	true,
};

inline void
SoftwareMixer::SetFilter(Filter *_filter) noexcept
{
	filter = _filter;

	if (filter != nullptr)
		volume_filter_set(filter,
				  PercentVolumeToSoftwareVolume(volume));
}

void
software_mixer_set_filter(Mixer &mixer, Filter *filter) noexcept
{
	auto &sm = (SoftwareMixer &)mixer;
	sm.SetFilter(filter);
}
