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

#include <stdlib.h>

#define AUDIO_DEVICE_STATE "audio_device_state:"

unsigned audio_output_state_version;

void
audio_output_state_save(BufferedOutputStream &os,
			const MultipleOutputs &outputs)
{
	for (unsigned i = 0, n = outputs.Size(); i != n; ++i) {
		const auto &ao = outputs.Get(i);
		const std::scoped_lock<Mutex> lock(ao.mutex);

		os.Format(AUDIO_DEVICE_STATE "%d:%s\n",
			  ao.IsEnabled(), ao.GetName());
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
			 "Ignoring device state for '{}'", name);
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
