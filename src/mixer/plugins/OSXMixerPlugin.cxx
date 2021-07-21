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

#include "mixer/MixerInternal.hxx"
#include "output/plugins/OSXOutputPlugin.hxx"

class OSXMixer final : public Mixer {
	OSXOutput &output;

public:
	OSXMixer(OSXOutput &_output, MixerListener &_listener)
		:Mixer(osx_mixer_plugin, _listener),
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
OSXMixer::GetVolume()
{
	return osx_output_get_volume(output);
}

void
OSXMixer::SetVolume(unsigned new_volume)
{
	osx_output_set_volume(output, new_volume);
}

static Mixer *
osx_mixer_init([[maybe_unused]] EventLoop &event_loop, AudioOutput &ao,
		MixerListener &listener,
		[[maybe_unused]] const ConfigBlock &block)
{
	OSXOutput &osxo = (OSXOutput &)ao;
	return new OSXMixer(osxo, listener);
}

const MixerPlugin osx_mixer_plugin = {
	osx_mixer_init,
	true,
};
