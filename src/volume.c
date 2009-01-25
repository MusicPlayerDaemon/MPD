/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "volume.h"
#include "conf.h"
#include "player_control.h"
#include "idle.h"
#include "pcm_volume.h"
#include "config.h"
#include "audio.h"

#include <glib.h>

#include <math.h>
#include <string.h>
#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "volume"

#define VOLUME_MIXER_TYPE_SOFTWARE		0
#define VOLUME_MIXER_TYPE_HARDWARE		1

#define VOLUME_MIXER_SOFTWARE_DEFAULT		""
#define SW_VOLUME_STATE                         "sw_volume: "

const struct audio_output_plugin *default_mixer;
static int volume_mixer_type = VOLUME_MIXER_TYPE_HARDWARE;
static int volume_software_set = 100;

void volume_finish(void)
{
}

/**
 * Finds the first audio_output configuration section with the
 * specified type.
 */
static struct config_param *
find_output_config(const char *type)
{
	struct config_param *param = NULL;

	while ((param = config_get_next_param(CONF_AUDIO_OUTPUT,
					      param)) != NULL) {
		const char *param_type =
			config_get_block_string(param, "type", NULL);
		if (param_type != NULL && strcmp(param_type, type) == 0)
			return param;
	}

	return NULL;
}

/**
 * Copy a (top-level) legacy mixer configuration parameter to the
 * audio_output section.
 */
static void
mixer_copy_legacy_param(const char *type, const char *name)
{
	const struct config_param *param;
	struct config_param *output;
	const struct block_param *bp;

	/* see if the deprecated configuration exists */

	param = config_get_param(name);
	if (param == NULL)
		return;

	g_warning("deprecated option '%s' found, moving to '%s' audio output",
		  name, type);

	/* determine the configuration section */

	output = find_output_config(type);
	if (output == NULL) {
		/* if there is no output configuration at all, create
		   a new and empty configuration section for the
		   legacy mixer */

		if (config_get_next_param(CONF_AUDIO_OUTPUT, NULL) != NULL)
			/* there is an audio_output configuration, but
			   it does not match the mixer_type setting */
			g_error("no '%s' audio output found", type);

		output = newConfigParam(NULL, param->line);
		addBlockParam(output, "type", type, param->line);
		addBlockParam(output, "name", type, param->line);
		config_add_param(CONF_AUDIO_OUTPUT, output);
	}

	bp = getBlockParam(output, name);
	if (bp != NULL)
		g_error("the '%s' audio output already has a '%s' setting",
			type, name);

	/* duplicate the parameter in the configuration section */

	addBlockParam(output, name, param->value, param->line);
}

static void
mixer_reconfigure(const char *type)
{
	mixer_copy_legacy_param(type, CONF_MIXER_DEVICE);
	mixer_copy_legacy_param(type, CONF_MIXER_CONTROL);
}

void volume_init(void)
{
	const struct config_param *param = config_get_param(CONF_MIXER_TYPE);
	//hw mixing is by default
	if (param) {
		if (strcmp(param->value, VOLUME_MIXER_SOFTWARE) == 0) {
			volume_mixer_type = VOLUME_MIXER_TYPE_SOFTWARE;
		} else if (strcmp(param->value, VOLUME_MIXER_HARDWARE) == 0) {
			//nothing to do
		} else {
			//fallback to old config behaviour
			if (strcmp(param->value, VOLUME_MIXER_OSS) == 0) {
				mixer_reconfigure(param->value);
			} else if (strcmp(param->value, VOLUME_MIXER_ALSA) == 0) {
				mixer_reconfigure(param->value);
			} else {
				g_error("unknown mixer type %s at line %i\n",
					param->value, param->line);
			}
		}
	}
}

static int hardware_volume_get(void)
{
	int device, count;
	int volume, volume_total, volume_ok;

	volume_total = 0;
	volume_ok = 0;

	count = audio_output_count();
	for (device=0; device<count ;device++) {
		if (mixer_control_getvol(device, &volume)) {
			g_debug("device %d: volume: %d\n", device, volume);
			volume_total += volume;
			volume_ok++;
		}
	}
	if (volume_ok > 0) {
		//return average
		return volume_total / volume_ok;
	} else {
		return -1;
	}
}

static int software_volume_get(void)
{
	return volume_software_set;
}

int volume_level_get(void)
{
	switch (volume_mixer_type) {
	case VOLUME_MIXER_TYPE_SOFTWARE:
		return software_volume_get();
	case VOLUME_MIXER_TYPE_HARDWARE:
		return hardware_volume_get();
	default:
		return -1;
	}
	return -1;
}

static int software_volume_change(int change, int rel)
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

	return 0;
}

static int hardware_volume_change(int change, int rel)
{
	int device, count;

	count = audio_output_count();
	for (device=0; device<count ;device++) {
		mixer_control_setvol(device, change, rel);
	}
	return 0;
}

int volume_level_change(int change, int rel)
{
	idle_add(IDLE_MIXER);

	switch (volume_mixer_type) {
	case VOLUME_MIXER_TYPE_HARDWARE:
		return hardware_volume_change(change, rel);
	case VOLUME_MIXER_TYPE_SOFTWARE:
		return software_volume_change(change, rel);
	default:
		return 0;
	}
}

void read_sw_volume_state(FILE *fp)
{
	char buf[sizeof(SW_VOLUME_STATE) + sizeof("100") - 1];
	char *end = NULL;
	long int sv;

	if (volume_mixer_type != VOLUME_MIXER_TYPE_SOFTWARE)
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
	if (volume_mixer_type == VOLUME_MIXER_TYPE_SOFTWARE)
		fprintf(fp, SW_VOLUME_STATE "%d\n", volume_software_set);
}
