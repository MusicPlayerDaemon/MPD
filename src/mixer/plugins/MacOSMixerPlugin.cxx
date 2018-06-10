/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "mixer/MixerInternal.hxx"
#include "output/plugins/MacOSOutputPlugin.hxx"

class MacOSMixer final : public Mixer {
	MacOSOutput &output;

public:
	MacOSMixer(MacOSOutput &_output, MixerListener &_listener)
		:Mixer(macos_mixer_plugin, _listener),
		 output(_output)
	{
	}

	/* virtual methods from class Mixer */
	void Open() noexcept override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override;
	void SetVolume(unsigned volume) override;
};

int
MacOSMixer::GetVolume()
{
	return macos_output_get_volume(output);
}

void
MacOSMixer::SetVolume(unsigned new_volume)
{
	macos_output_set_volume(output, new_volume);
}

static Mixer *
macos_mixer_init(gcc_unused EventLoop &event_loop, AudioOutput &ao,
		MixerListener &listener,
		gcc_unused const ConfigBlock &block)
{
	MacOSOutput &osxo = (MacOSOutput &)ao;
	return new MacOSMixer(osxo, listener);
}

const MixerPlugin macos_mixer_plugin = {
	macos_mixer_init,
	true,
};
