/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

class NullMixer final : public Mixer {
	/**
	 * The current volume in percent (0..100).
	 */
	unsigned volume;

public:
	NullMixer(MixerListener &_listener)
		:Mixer(null_mixer_plugin, _listener),
		 volume(100)
	{
	}

	/* virtual methods from class Mixer */
	bool Open(gcc_unused Error &error) override {
		return true;
	}

	void Close() override {
	}

	int GetVolume(gcc_unused Error &error) override {
		return volume;
	}

	bool SetVolume(unsigned _volume, gcc_unused Error &error) override {
		volume = _volume;
		return true;
	}
};

static Mixer *
null_mixer_init(gcc_unused EventLoop &event_loop,
		gcc_unused AudioOutput &ao,
		MixerListener &listener,
		gcc_unused const ConfigBlock &block,
		gcc_unused Error &error)
{
	return new NullMixer(listener);
}

const MixerPlugin null_mixer_plugin = {
	null_mixer_init,
	true,
};
