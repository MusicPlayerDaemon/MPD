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

#include "volume.h"
#include "conf.h"
#include "player_control.h"
#include "idle.h"
#include "pcm_volume.h"
#include "config.h"
#include "output_all.h"
#include "mixer_control.h"
#include "mixer_all.h"
#include "mixer_type.h"

#include <glib.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "volume"

#define SW_VOLUME_STATE                         "sw_volume: "

static enum mixer_type volume_mixer_type = MIXER_TYPE_HARDWARE;

static int volume_software_set = 100;

/** the cached hardware mixer value; invalid if negative */
static int last_hardware_volume = -1;
/** the age of #last_hardware_volume */
static GTimer *hardware_volume_timer;

void volume_finish(void)
{
	if (volume_mixer_type == MIXER_TYPE_HARDWARE)
		g_timer_destroy(hardware_volume_timer);
}

void volume_init(void)
{
	const struct config_param *param = config_get_param(CONF_MIXER_TYPE);
	//hw mixing is by default
	if (param) {
		volume_mixer_type = mixer_type_parse(param->value);
		switch (volume_mixer_type) {
		case MIXER_TYPE_NONE:
		case MIXER_TYPE_SOFTWARE:
			mixer_disable_all();
			break;

		case MIXER_TYPE_HARDWARE:
			//nothing to do
			break;

		case MIXER_TYPE_UNKNOWN:
			g_error("unknown mixer type %s at line %i\n",
				param->value, param->line);
		}
	}

	if (volume_mixer_type == MIXER_TYPE_HARDWARE)
		hardware_volume_timer = g_timer_new();
}

static int hardware_volume_get(void)
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

static int software_volume_get(void)
{
	return volume_software_set;
}

int volume_level_get(void)
{
	switch (volume_mixer_type) {
	case MIXER_TYPE_SOFTWARE:
		return software_volume_get();
	case MIXER_TYPE_HARDWARE:
		return hardware_volume_get();
	case MIXER_TYPE_NONE:
	case MIXER_TYPE_UNKNOWN:
		return -1;
	}

	/* unreachable */
	assert(false);
	return -1;
}

static bool software_volume_change(int change, bool rel)
{
	int new = change;

	if (rel)
		new += volume_software_set;

	if (new > 100)
		new = 100;
	else if (new < 0)
		new = 0;

	volume_software_set = new;

	/*new = 100.0*(exp(new/50.0)-1)/(M_E*M_E-1)+0.5; */
	if (new >= 100)
		new = PCM_VOLUME_1;
	else if (new <= 0)
		new = 0;
	else
		new = pcm_float_to_volume((exp(new / 25.0) - 1) /
					  (54.5981500331F - 1));

	setPlayerSoftwareVolume(new);

	return true;
}

static bool hardware_volume_change(int change, bool rel)
{
	/* reset the cache */
	last_hardware_volume = -1;

	return mixer_all_set_volume(change, rel);
}

bool volume_level_change(int change, bool rel)
{
	idle_add(IDLE_MIXER);

	switch (volume_mixer_type) {
	case MIXER_TYPE_HARDWARE:
		return hardware_volume_change(change, rel);
	case MIXER_TYPE_SOFTWARE:
		return software_volume_change(change, rel);
	default:
		return true;
	}
}

void read_sw_volume_state(FILE *fp)
{
	char buf[sizeof(SW_VOLUME_STATE) + sizeof("100") - 1];
	char *end = NULL;
	long int sv;

	if (volume_mixer_type != MIXER_TYPE_SOFTWARE)
		return;
	while (fgets(buf, sizeof(buf), fp)) {
		if (!g_str_has_prefix(buf, SW_VOLUME_STATE))
			continue;

		g_strchomp(buf);
		sv = strtol(buf + strlen(SW_VOLUME_STATE), &end, 10);
		if (G_LIKELY(!*end))
			software_volume_change(sv, 0);
		else
			g_warning("Can't parse software volume: %s\n", buf);
		return;
	}
}

void save_sw_volume_state(FILE *fp)
{
	if (volume_mixer_type == MIXER_TYPE_SOFTWARE)
		fprintf(fp, SW_VOLUME_STATE "%d\n", volume_software_set);
}
