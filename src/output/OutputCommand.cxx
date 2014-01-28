/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

#include "config.h"
#include "OutputCommand.hxx"
#include "MultipleOutputs.hxx"
#include "Internal.hxx"
#include "PlayerControl.hxx"
#include "mixer/MixerControl.hxx"
#include "Idle.hxx"

extern unsigned audio_output_state_version;

bool
audio_output_enable_index(MultipleOutputs &outputs, unsigned idx)
{
	if (idx >= outputs.Size())
		return false;

	AudioOutput &ao = outputs.Get(idx);
	if (ao.enabled)
		return true;

	ao.enabled = true;
	idle_add(IDLE_OUTPUT);

	ao.player_control->UpdateAudio();

	++audio_output_state_version;

	return true;
}

bool
audio_output_disable_index(MultipleOutputs &outputs, unsigned idx)
{
	if (idx >= outputs.Size())
		return false;

	AudioOutput &ao = outputs.Get(idx);
	if (!ao.enabled)
		return true;

	ao.enabled = false;
	idle_add(IDLE_OUTPUT);

	Mixer *mixer = ao.mixer;
	if (mixer != nullptr) {
		mixer_close(mixer);
		idle_add(IDLE_MIXER);
	}

	ao.player_control->UpdateAudio();

	++audio_output_state_version;

	return true;
}

bool
audio_output_toggle_index(MultipleOutputs &outputs, unsigned idx)
{
	if (idx >= outputs.Size())
		return false;

	AudioOutput &ao = outputs.Get(idx);
	const bool enabled = ao.enabled = !ao.enabled;
	idle_add(IDLE_OUTPUT);

	if (!enabled) {
		Mixer *mixer = ao.mixer;
		if (mixer != nullptr) {
			mixer_close(mixer);
			idle_add(IDLE_MIXER);
		}
	}

	ao.player_control->UpdateAudio();

	++audio_output_state_version;

	return true;
}
