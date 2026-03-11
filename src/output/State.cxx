// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Saving and loading the audio output states to/from the state file.
 *
 */

#include "State.hxx"
#include "MultipleOutputs.hxx"
#include "Domain.hxx"
#include "Log.hxx"
#include "io/BufferedOutputStream.hxx"
#include "util/StringCompare.hxx"
#include "Partition.hxx"
#include "config/PartitionConfig.hxx"

#include <fmt/format.h>

#include <stdlib.h>

#define AUDIO_DEVICE_STATE "audio_device_state:"
#define DEFAULT_PARTITION "default"

unsigned audio_output_state_version;

/**
 * Iterate the instance and save audio output state configuration.
 *
 * Writes the name of the audio outputs, the partitions they are assigned to,
 * and the state of the output.
 *
 * Writes a configuration line this format:
 *   AUDIO_DEVICE_STATE<value>:<device>:<partition>
 *
 * Where:
 *   <value> = 0 (disabled) or 1 (enabled)
 *   <device> = output device name
 *
 * @param os The output stream
 * @param outputs The outputs assigned to this partition
 * @return nothing
 */
void
audio_output_state_save(BufferedOutputStream &os,
			const MultipleOutputs &outputs)
{
	for (unsigned i = 0, n = outputs.Size(); i != n; ++i) {
		const auto &ao = outputs.Get(i);
		const std::scoped_lock lock{ao.mutex};

		os.Fmt(AUDIO_DEVICE_STATE "{}:{}\n",
		       (unsigned)ao.IsEnabled(), ao.GetName());
	}
}

/**
 * Parse and apply audio output state configuration.
 *
 * Reads a configuration line in one of these formats:
 *   AUDIO_DEVICE_STATE<value>:<device>
 *   AUDIO_DEVICE_STATE<value>:<device>:<partition>
 *
 * Where:
 *   <value> = 0 (disabled) or 1 (enabled)
 *   <device> = output device name
 *   <partition> = optional partition name
 *
 * @param line The configuration line to parse
 * @param outputs The collection of audio outputs to modify
 * @param current_partition The partition to which the outputs belong
 * @return true if the line was valid and processed, false on parse error
 */
bool
audio_output_state_read(const char *line, MultipleOutputs &outputs, Partition *current_partition)
{
	long value;
	char *endptr;
	const char *name;

	line = StringAfterPrefix(line, AUDIO_DEVICE_STATE);
	if (line == nullptr)
		return false;

	value = strtol(line, &endptr, 10);
	if (*endptr != ':' || (value != 0 && value != 1))
		return false;

	name = endptr + 1;
	auto *ao = outputs.FindByName(name);
	if (ao == nullptr) {
		FmtDebug(output_domain,
			 "Ignoring device state for {:?}", name);
		return true;
	}

	if (current_partition->name != DEFAULT_PARTITION) {
		// Move the output to this partition
		FmtDebug(output_domain,
				"Moving device {:?} from default to partition {:?}",
				name, current_partition->name);
		current_partition->outputs.AddMoveFrom(std::move(*ao), value != 0);
		return true;
	}

	ao->LockSetEnabled(value != 0);
	return true;
}

unsigned
audio_output_state_get_version()
{
	return audio_output_state_version;
}
