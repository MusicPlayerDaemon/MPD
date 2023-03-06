// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "NullMixerPlugin.hxx"
#include "mixer/Mixer.hxx"

class NullMixer final : public Mixer {
	/**
	 * The current volume in percent (0..100).
	 */
	unsigned volume;

public:
	explicit NullMixer(MixerListener &_listener)
		:Mixer(null_mixer_plugin, _listener),
		 volume(100)
	{
	}

	/* virtual methods from class Mixer */
	void Open() override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override {
		return volume;
	}

	void SetVolume(unsigned _volume) override {
		volume = _volume;
	}
};

static Mixer *
null_mixer_init([[maybe_unused]] EventLoop &event_loop,
		[[maybe_unused]] AudioOutput &ao,
		MixerListener &listener,
		[[maybe_unused]] const ConfigBlock &block)
{
	return new NullMixer(listener);
}

const MixerPlugin null_mixer_plugin = {
	null_mixer_init,
	true,
};
