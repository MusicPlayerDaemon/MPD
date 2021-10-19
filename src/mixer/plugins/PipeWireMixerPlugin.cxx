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

#include "PipeWireMixerPlugin.hxx"
#include "mixer/MixerInternal.hxx"
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
