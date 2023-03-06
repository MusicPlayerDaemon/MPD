// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PipeWireMixerPlugin.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/Listener.hxx"
#include "output/plugins/PipeWireOutputPlugin.hxx"

#include <cmath>

class PipeWireMixer final : public Mixer {
	PipeWireOutput &output;

	int volume = 100;

public:
	PipeWireMixer(PipeWireOutput &_output,
		      MixerListener &_listener) noexcept
		:Mixer(pipewire_mixer_plugin, _listener),
		 output(_output)
	{
	}

	~PipeWireMixer() noexcept override;

	PipeWireMixer(const PipeWireMixer &) = delete;
	PipeWireMixer &operator=(const PipeWireMixer &) = delete;

	void OnVolumeChanged(float new_volume) noexcept {
		volume = std::lround(new_volume * 100.f);

		listener.OnMixerVolumeChanged(*this, volume);
	}

	/* virtual methods from class Mixer */
	void Open() override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override;
	void SetVolume(unsigned volume) override;
};

void
pipewire_mixer_on_change(PipeWireMixer &pm, float new_volume) noexcept
{
	pm.OnVolumeChanged(new_volume);
}

int
PipeWireMixer::GetVolume()
{
	return volume;
}

void
PipeWireMixer::SetVolume(unsigned new_volume)
{
	pipewire_output_set_volume(output, float(new_volume) * 0.01f);
	volume = new_volume;
}

static Mixer *
pipewire_mixer_init([[maybe_unused]] EventLoop &event_loop, AudioOutput &ao,
		    MixerListener &listener,
		    const ConfigBlock &)
{
	auto &po = (PipeWireOutput &)ao;
	auto *pm = new PipeWireMixer(po, listener);
	pipewire_output_set_mixer(po, *pm);
	return pm;
}

PipeWireMixer::~PipeWireMixer() noexcept
{
	pipewire_output_clear_mixer(output, *this);
}

const MixerPlugin pipewire_mixer_plugin = {
	pipewire_mixer_init,
	true,
};
