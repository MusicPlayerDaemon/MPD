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
