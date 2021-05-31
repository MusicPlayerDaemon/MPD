/*
 * Copyright 2017 Christopher Zimmermann <christopher@gmerlin.de>
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

#include "mixer/MixerInternal.hxx"
#include "output/plugins/SndioOutputPlugin.hxx"

class SndioMixer final : public Mixer {
	SndioOutput &output;

public:
	SndioMixer(SndioOutput &_output, MixerListener &_listener)
		:Mixer(sndio_mixer_plugin, _listener), output(_output)
	{
		output.RegisterMixerListener(this, &_listener);
	}

	/* virtual methods from class Mixer */
	void Open() override {}

	void Close() noexcept override {}

	int GetVolume() override {
		return output.GetVolume();
	}

	void SetVolume(unsigned volume) override {
		output.SetVolume(volume);
	}

};

static Mixer *
sndio_mixer_init([[maybe_unused]] EventLoop &event_loop,
		AudioOutput &ao,
		MixerListener &listener,
		[[maybe_unused]] const ConfigBlock &block)
{
	return new SndioMixer((SndioOutput &)ao, listener);
}

constexpr MixerPlugin sndio_mixer_plugin = {
	sndio_mixer_init,
	false,
};
