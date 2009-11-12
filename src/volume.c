/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "volume.h"
#include "conf.h"
#include "player_control.h"
#include "idle.h"
#include "pcm_volume.h"
#include "output_all.h"
#include "mixer_control.h"
#include "mixer_all.h"
#include "mixer_type.h"
#include "event_pipe.h"

#include <glib.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "volume"

#define SW_VOLUME_STATE                         "sw_volume: "

static unsigned volume_software_set = 100;

/** the cached hardware mixer value; invalid if negative */
static int last_hardware_volume = -1;
/** the age of #last_hardware_volume */
static GTimer *hardware_volume_timer;

/**
 * Handler for #PIPE_EVENT_MIXER.
 */
static void
mixer_event_callback(void)
{
	/* flush the hardware volume cache */
	last_hardware_volume = -1;

	/* notify clients */
	idle_add(IDLE_MIXER);
}

void volume_finish(void)
{
	g_timer_destroy(hardware_volume_timer);
}

void volume_init(void)
{
	hardware_volume_timer = g_timer_new();

	event_pipe_register(PIPE_EVENT_MIXER, mixer_event_callback);
}

int volume_level_get(void)
{
	assert(hardware_volume_timer != NULL);

	if (last_hardware_volume >= 0 &&
	    g_timer_elapsed(hardware_volume_timer, NULL) < 1.0)
		/* throttle access to hardware mixers */
		return last_hardware_volume;

	last_hardware_volume = mixer_all_get_volume();
	g_timer_start(hardware_volume_timer);
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
	char *end = NULL;
	long int sv;

	if (!g_str_has_prefix(line, SW_VOLUME_STATE))
		return false;

	line += sizeof(SW_VOLUME_STATE) - 1;
	sv = strtol(line, &end, 10);
	if (*end == 0 && sv >= 0 && sv <= 100)
		software_volume_change(sv);
	else
		g_warning("Can't parse software volume: %s\n", line);
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
