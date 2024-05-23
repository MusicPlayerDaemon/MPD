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

#include <fmt/format.h>

#include <stdlib.h>

#define AUDIO_DEVICE_STATE "audio_device_state:"

unsigned audio_output_state_version;

void
audio_output_state_save(BufferedOutputStream &os,
			const MultipleOutputs &outputs)
{
	for (unsigned i = 0, n = outputs.Size(); i != n; ++i) {
		const auto &ao = outputs.Get(i);
		const std::scoped_lock lock{ao.mutex};

		os.Fmt(FMT_STRING(AUDIO_DEVICE_STATE "{}:{}\n"),
		       (unsigned)ao.IsEnabled(), ao.GetName());
	}
}

bool
audio_output_state_read(const char *line, MultipleOutputs &outputs)
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

	if (value != 0)
		/* state is "enabled": no-op */
		return true;

	name = endptr + 1;
	auto *ao = outputs.FindByName(name);
	if (ao == nullptr) {
		FmtDebug(output_domain,
			 "Ignoring device state for {:?}", name);
		return true;
	}

	ao->LockSetEnabled(false);
	return true;
}

unsigned
audio_output_state_get_version()
{
	return audio_output_state_version;
}
