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

#include "Volume.hxx"
#include "output/MultipleOutputs.hxx"
#include "Idle.hxx"
#include "util/StringCompare.hxx"
#include "util/Domain.hxx"
#include "system/PeriodClock.hxx"
#include "io/BufferedOutputStream.hxx"
#include "Log.hxx"

#include <cassert>

#include <stdlib.h>

#define SW_VOLUME_STATE                         "sw_volume: "

static constexpr Domain volume_domain("volume");

static unsigned volume_software_set = 100;

/** the cached hardware mixer value; invalid if negative */
static int last_hardware_volume = -1;
/** the age of #last_hardware_volume */
static PeriodClock hardware_volume_clock;

void
InvalidateHardwareVolume() noexcept
{
	/* flush the hardware volume cache */
	last_hardware_volume = -1;
}

int
volume_level_get(const MultipleOutputs &outputs) noexcept
{
	if (last_hardware_volume >= 0 &&
	    !hardware_volume_clock.CheckUpdate(std::chrono::seconds(1)))
		/* throttle access to hardware mixers */
		return last_hardware_volume;

	last_hardware_volume = outputs.GetVolume();
	return last_hardware_volume;
}

static bool
software_volume_change(MultipleOutputs &outputs, unsigned volume)
{
	assert(volume <= 100);

	volume_software_set = volume;
	outputs.SetSoftwareVolume(volume);

	return true;
}

static bool
hardware_volume_change(MultipleOutputs &outputs, unsigned volume)
{
	/* reset the cache */
	last_hardware_volume = -1;

	return outputs.SetVolume(volume);
}

bool
volume_level_change(MultipleOutputs &outputs, unsigned volume)
{
	assert(volume <= 100);

	volume_software_set = volume;

	idle_add(IDLE_MIXER);

	return hardware_volume_change(outputs, volume);
}

bool
read_sw_volume_state(const char *line, MultipleOutputs &outputs)
{
	char *end = nullptr;
	long int sv;

	line = StringAfterPrefix(line, SW_VOLUME_STATE);
	if (line == nullptr)
		return false;

	sv = strtol(line, &end, 10);
	if (*end == 0 && sv >= 0 && sv <= 100)
		software_volume_change(outputs, sv);
	else
		FmtWarning(volume_domain,
			   "Can't parse software volume: {}", line);
	return true;
}

void
save_sw_volume_state(BufferedOutputStream &os)
{
	os.Format(SW_VOLUME_STATE "%u\n", volume_software_set);
}

unsigned
sw_volume_state_get_hash() noexcept
{
	return volume_software_set;
}
