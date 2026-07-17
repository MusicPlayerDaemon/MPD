// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

#include "OutputCommand.hxx"
#include "Client.hxx"
#include "Control.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/Memento.hxx"
#include "mixer/Listener.hxx"
#include "protocol/Ack.hxx"
#include "protocol/IdleFlags.hxx"
#include "Partition.hxx"

extern unsigned audio_output_state_version;

void
audio_output_enable_index(Partition &partition,
			  unsigned idx)
{
	auto &outputs = partition.outputs;
	if (idx >= outputs.Size())
		throw ProtocolError(ACK_ERROR_NO_EXIST, "No such audio output");

	auto &ao = outputs.Get(idx);
	if (!ao.LockSetEnabled(true))
		return;

	partition.EmitIdle(IDLE_OUTPUT);

	auto *mixer = ao.GetMixer();
	if (mixer != nullptr) {
		partition.mixer_memento.InvalidateHardwareVolume();
		mixer->listener.OnMixerChanged();
	}

	ao.GetClient().ApplyEnabled();

	++audio_output_state_version;
}

void
audio_output_disable_index(Partition &partition,
			   unsigned idx)
{
	auto &outputs = partition.outputs;
	if (idx >= outputs.Size())
		throw ProtocolError(ACK_ERROR_NO_EXIST, "No such audio output");

	auto &ao = outputs.Get(idx);
	if (!ao.LockSetEnabled(false))
		return;

	partition.EmitIdle(IDLE_OUTPUT);

	auto *mixer = ao.GetMixer();
	if (mixer != nullptr) {
		mixer->LockClose();
		partition.mixer_memento.InvalidateHardwareVolume();
		mixer->listener.OnMixerChanged();
	}

	ao.GetClient().ApplyEnabled();

	++audio_output_state_version;
}

void
audio_output_toggle_index(Partition &partition,
			  unsigned idx)
{
	auto &outputs = partition.outputs;
	if (idx >= outputs.Size())
		throw ProtocolError(ACK_ERROR_NO_EXIST, "No such audio output");

	auto &ao = outputs.Get(idx);
	const bool enabled = ao.LockToggleEnabled();
	partition.EmitIdle(IDLE_OUTPUT);

	if (!enabled) {
		auto *mixer = ao.GetMixer();
		if (mixer != nullptr) {
			mixer->LockClose();
			partition.mixer_memento.InvalidateHardwareVolume();
			mixer->listener.OnMixerChanged();
		}
	}

	ao.GetClient().ApplyEnabled();

	++audio_output_state_version;
}
