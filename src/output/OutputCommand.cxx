// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

#include "OutputCommand.hxx"
#include "MultipleOutputs.hxx"
#include "Client.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/Memento.hxx"
#include "mixer/Listener.hxx"
#include "Idle.hxx"

extern unsigned audio_output_state_version;

bool
audio_output_enable_index(MultipleOutputs &outputs,
			  MixerMemento &mixer_memento,
			  unsigned idx)
{
	if (idx >= outputs.Size())
		return false;

	auto &ao = outputs.Get(idx);
	if (!ao.LockSetEnabled(true))
		return true;

	idle_add(IDLE_OUTPUT);

	auto *mixer = ao.GetMixer();
	if (mixer != nullptr) {
		mixer_memento.InvalidateHardwareVolume();
		mixer->listener.OnMixerChanged();
	}

	ao.GetClient().ApplyEnabled();

	++audio_output_state_version;

	return true;
}

bool
audio_output_disable_index(MultipleOutputs &outputs,
			   MixerMemento &mixer_memento,
			   unsigned idx)
{
	if (idx >= outputs.Size())
		return false;

	auto &ao = outputs.Get(idx);
	if (!ao.LockSetEnabled(false))
		return true;

	idle_add(IDLE_OUTPUT);

	auto *mixer = ao.GetMixer();
	if (mixer != nullptr) {
		mixer->LockClose();
		mixer_memento.InvalidateHardwareVolume();
		mixer->listener.OnMixerChanged();
	}

	ao.GetClient().ApplyEnabled();

	++audio_output_state_version;

	return true;
}

bool
audio_output_toggle_index(MultipleOutputs &outputs,
			  MixerMemento &mixer_memento,
			  unsigned idx)
{
	if (idx >= outputs.Size())
		return false;

	auto &ao = outputs.Get(idx);
	const bool enabled = ao.LockToggleEnabled();
	idle_add(IDLE_OUTPUT);

	if (!enabled) {
		auto *mixer = ao.GetMixer();
		if (mixer != nullptr) {
			mixer->LockClose();
			mixer_memento.InvalidateHardwareVolume();
			mixer->listener.OnMixerChanged();
		}
	}

	ao.GetClient().ApplyEnabled();

	++audio_output_state_version;

	return true;
}
