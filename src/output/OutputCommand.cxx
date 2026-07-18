// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

#include "OutputCommand.hxx"
#include "AllOutputs.hxx"
#include "Client.hxx"
#include "Control.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/Memento.hxx"
#include "mixer/Listener.hxx"
#include "protocol/Ack.hxx"
#include "protocol/IdleFlags.hxx"
#include "Partition.hxx"
#include "Instance.hxx"

extern unsigned audio_output_state_version;

AudioOutputControl &
CheckPartitionOutput(Partition &partition, unsigned idx)
{
	auto &all_outputs = partition.instance.outputs;
	if (idx >= all_outputs.Size())
		throw ProtocolError(ACK_ERROR_NO_EXIST, "No such audio output");

	auto &ao = all_outputs.Get(idx);
	if (!partition.outputs.Owns(ao))
		throw ProtocolError{ACK_ERROR_NO_EXIST, "Audio output not in this partition"};

	return ao;
}

/**
 * Like CheckPartitionOutput(), but move the output to this partition.
 */
static AudioOutputControl &
CheckMovePartitionOutput(Partition &partition, unsigned idx, bool enable)
{
	auto &all_outputs = partition.instance.outputs;
	if (idx >= all_outputs.Size())
		throw ProtocolError(ACK_ERROR_NO_EXIST, "No such audio output");

	auto &ao = all_outputs.Get(idx);
	if (!partition.outputs.Owns(ao)) {
		/* find the partition which owns this output currently */
		auto *src_partition = partition.instance.FindOwningPartition(ao);
		assert(src_partition != &partition);

		if (src_partition != nullptr) {
			src_partition->outputs.ReleaseOwnership(ao);
			src_partition->EmitIdle(IDLE_OUTPUT);
		}

		partition.outputs.AcquireOwnership(ao, enable,
						   partition.replay_gain_mode);

		partition.EmitIdle(IDLE_OUTPUT);
	}

	return ao;
}

void
audio_output_enable_index(Partition &partition,
			  unsigned idx)
{
	auto &ao = CheckMovePartitionOutput(partition, idx, true);
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
	auto &ao = CheckPartitionOutput(partition, idx);
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
	auto &ao = CheckPartitionOutput(partition, idx);
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
