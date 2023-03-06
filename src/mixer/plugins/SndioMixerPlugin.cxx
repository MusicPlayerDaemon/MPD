// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright Christopher Zimmermann <christopher@gmerlin.de>

#include "SndioMixerPlugin.hxx"
#include "mixer/Mixer.hxx"
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
