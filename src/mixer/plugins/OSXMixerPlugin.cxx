// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OSXMixerPlugin.hxx"
#include "mixer/Mixer.hxx"
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
