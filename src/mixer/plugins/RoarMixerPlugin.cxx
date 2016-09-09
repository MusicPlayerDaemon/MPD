/*
 * Copyright 2003-2016 The Music Player Daemon Project
 * Copyright (C) 2010-2011 Philipp 'ph3-der-loewe' Schafft
 * Copyright (C) 2010-2011 Hans-Kristian 'maister' Arntzen
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
#include "output/plugins/RoarOutputPlugin.hxx"
#include "Compiler.h"

class RoarMixer final : public Mixer {
	/** the base mixer class */
	RoarOutput &self;

public:
	RoarMixer(RoarOutput &_output, MixerListener &_listener)
		:Mixer(roar_mixer_plugin, _listener),
		 self(_output) {}

	/* virtual methods from class Mixer */
	void Open() override {
	}

	virtual void Close() override {
	}

	int GetVolume() override;
	void SetVolume(unsigned volume) override;
};

static Mixer *
roar_mixer_init(gcc_unused EventLoop &event_loop, AudioOutput &ao,
		MixerListener &listener,
		gcc_unused const ConfigBlock &block)
{
	return new RoarMixer((RoarOutput &)ao, listener);
}

int
RoarMixer::GetVolume()
{
	return roar_output_get_volume(self);
}

void
RoarMixer::SetVolume(unsigned volume)
{
	roar_output_set_volume(self, volume);
}

const MixerPlugin roar_mixer_plugin = {
	roar_mixer_init,
	false,
};
