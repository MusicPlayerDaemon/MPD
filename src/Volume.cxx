/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "Volume.hxx"
#include "MixerAll.hxx"
#include "Idle.hxx"
#include "GlobalEvents.hxx"
#include "util/Domain.hxx"
#include "system/PeriodClock.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>

#define SW_VOLUME_STATE                         "sw_volume: "

static constexpr Domain volume_domain("volume");

static unsigned volume_software_set = 100;

/** the cached hardware mixer value; invalid if negative */
static int last_hardware_volume = -1;
/** the age of #last_hardware_volume */
static PeriodClock hardware_volume_clock;

/**
 * Handler for #GlobalEvents::MIXER.
 */
static void
mixer_event_callback(void)
{
	/* flush the hardware volume cache */
	last_hardware_volume = -1;

	/* notify clients */
	idle_add(IDLE_MIXER);
}

void volume_init(void)
{
	GlobalEvents::Register(GlobalEvents::MIXER, mixer_event_callback);
}

int volume_level_get(void)
{
	if (last_hardware_volume >= 0 &&
	    !hardware_volume_clock.CheckUpdate(1000))
		/* throttle access to hardware mixers */
		return last_hardware_volume;

	last_hardware_volume = mixer_all_get_volume();
	return last_hardware_volume;
}

static bool software_volume_change(unsigned volume)
{
	assert(volume <= 100);

	volume_software_set = volume;
	mixer_all_set_software_volume(volume);

	return true;
}

static bool hardware_volume_change(unsigned volume)
{
	/* reset the cache */
	last_hardware_volume = -1;

	return mixer_all_set_volume(volume);
}

bool volume_level_change(unsigned volume)
{
	assert(volume <= 100);

	volume_software_set = volume;

	idle_add(IDLE_MIXER);

	return hardware_volume_change(volume);
}

bool
read_sw_volume_state(const char *line)
{
	char *end = nullptr;
	long int sv;

	if (!g_str_has_prefix(line, SW_VOLUME_STATE))
		return false;

	line += sizeof(SW_VOLUME_STATE) - 1;
	sv = strtol(line, &end, 10);
	if (*end == 0 && sv >= 0 && sv <= 100)
		software_volume_change(sv);
	else
		FormatWarning(volume_domain,
			      "Can't parse software volume: %s", line);
	return true;
}

void save_sw_volume_state(FILE *fp)
{
	fprintf(fp, SW_VOLUME_STATE "%u\n", volume_software_set);
}

unsigned
sw_volume_state_get_hash(void)
{
	return volume_software_set;
}
