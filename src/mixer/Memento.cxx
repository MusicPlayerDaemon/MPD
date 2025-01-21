// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Memento.hxx"
#include "output/MultipleOutputs.hxx"
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
	os.Fmt(SW_VOLUME_STATE "{}\n", volume_software_set);
}
