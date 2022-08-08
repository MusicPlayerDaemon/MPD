/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "Memento.hxx"
#include "output/MultipleOutputs.hxx"
#include "Idle.hxx"
#include "util/StringCompare.hxx"
#include "io/BufferedOutputStream.hxx"

#include <fmt/format.h>

#include <cassert>

#include <stdlib.h>

#define SW_VOLUME_STATE                         "sw_volume: "

int
MixerMemento::GetVolume(const MultipleOutputs &outputs) noexcept
{
	if (last_hardware_volume >= 0 &&
	    !hardware_volume_clock.CheckUpdate(std::chrono::seconds(1)))
		/* throttle access to hardware mixers */
		return last_hardware_volume;

	last_hardware_volume = outputs.GetVolume();
	return last_hardware_volume;
}

inline bool
MixerMemento::SetSoftwareVolume(MultipleOutputs &outputs, unsigned volume)
{
	assert(volume <= 100);

	volume_software_set = volume;
	outputs.SetSoftwareVolume(volume);

	return true;
}

inline void
MixerMemento::SetHardwareVolume(MultipleOutputs &outputs, unsigned volume)
{
	/* reset the cache */
	last_hardware_volume = -1;

	outputs.SetVolume(volume);
}

void
MixerMemento::SetVolume(MultipleOutputs &outputs, unsigned volume)
{
	assert(volume <= 100);

	volume_software_set = volume;

	SetHardwareVolume(outputs, volume);
}

bool
MixerMemento::LoadSoftwareVolumeState(const char *line, MultipleOutputs &outputs)
{
	char *end = nullptr;
	long int sv;

	line = StringAfterPrefix(line, SW_VOLUME_STATE);
	if (line == nullptr)
		return false;

	sv = strtol(line, &end, 10);
	if (*end == 0 && sv >= 0 && sv <= 100)
		SetSoftwareVolume(outputs, sv);

	return true;
}

void
MixerMemento::SaveSoftwareVolumeState(BufferedOutputStream &os) const
{
	os.Fmt(FMT_STRING(SW_VOLUME_STATE "{}\n"), volume_software_set);
}
