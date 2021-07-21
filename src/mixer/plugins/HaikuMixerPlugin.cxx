/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * Copyright (C) 2010-2011 Philipp 'ph3-der-loewe' Schafft
 * Copyright (C) 2010-2011 Hans-Kristian 'maister' Arntzen
 * Copyright (C) 2014-2015 François 'mmu_man' Revol
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
#include "output/plugins/HaikuOutputPlugin.hxx"
#include "util/Compiler.h"

#include "util/RuntimeError.hxx"

class HaikuMixer final : public Mixer {
	/** the base mixer class */
	HaikuOutput &self;

public:
	HaikuMixer(HaikuOutput &_output, MixerListener &_listener)
		:Mixer(haiku_mixer_plugin, _listener),
		 self(_output) {}

	/* virtual methods from class Mixer */
	void Open() override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override;
	void SetVolume(unsigned volume) override;
};

static Mixer *
haiku_mixer_init([[maybe_unused]] EventLoop &event_loop, AudioOutput &ao,
		MixerListener &listener,
		[[maybe_unused]] const ConfigBlock &block)
{
	return new HaikuMixer((HaikuOutput &)ao, listener);
}

int
HaikuMixer::GetVolume()
{
	return haiku_output_get_volume(self);
}

void
HaikuMixer::SetVolume(unsigned volume)
{
	haiku_output_set_volume(self, volume);
}

const MixerPlugin haiku_mixer_plugin = {
	haiku_mixer_init,
	false,
};
