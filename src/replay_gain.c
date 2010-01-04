/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
 * (c)2004 replayGain code by AliasMrJones
 */

#include "config.h"
#include "replay_gain.h"
#include "conf.h"
#include "idle.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *const replay_gain_mode_names[] = {
	[REPLAY_GAIN_ALBUM] = "album",
	[REPLAY_GAIN_TRACK] = "track",
};

enum replay_gain_mode replay_gain_mode = REPLAY_GAIN_OFF;

float replay_gain_preamp = 1.0;
float replay_gain_missing_preamp = 1.0;

const char *
replay_gain_get_mode_string(void)
{
	switch (replay_gain_mode) {
	case REPLAY_GAIN_OFF:
		return "off";

	case REPLAY_GAIN_TRACK:
		return "track";

	case REPLAY_GAIN_ALBUM:
		return "album";
	}

	/* unreachable */
	assert(false);
	return "off";
}

bool
replay_gain_set_mode_string(const char *p)
{
	assert(p != NULL);

	if (strcmp(p, "off") == 0)
		replay_gain_mode = REPLAY_GAIN_OFF;
	else if (strcmp(p, "track") == 0)
		replay_gain_mode = REPLAY_GAIN_TRACK;
	else if (strcmp(p, "album") == 0)
		replay_gain_mode = REPLAY_GAIN_ALBUM;
	else
		return false;

	idle_add(IDLE_OPTIONS);

	return true;
}

void replay_gain_global_init(void)
{
	const struct config_param *param = config_get_param(CONF_REPLAYGAIN);

	if (param != NULL && !replay_gain_set_mode_string(param->value)) {
		g_error("replaygain value \"%s\" at line %i is invalid\n",
			param->value, param->line);
	}

	param = config_get_param(CONF_REPLAYGAIN_PREAMP);

	if (param) {
		char *test;
		float f = strtod(param->value, &test);

		if (*test != '\0') {
			g_error("Replaygain preamp \"%s\" is not a number at "
				"line %i\n", param->value, param->line);
		}

		if (f < -15 || f > 15) {
			g_error("Replaygain preamp \"%s\" is not between -15 and"
				"15 at line %i\n", param->value, param->line);
		}

		replay_gain_preamp = pow(10, f / 20.0);
	}

	param = config_get_param(CONF_REPLAYGAIN_MISSING_PREAMP);

	if (param) {
		char *test;
		float f = strtod(param->value, &test);

		if (*test != '\0') {
			g_error("Replaygain missing preamp \"%s\" is not a number at "
				"line %i\n", param->value, param->line);
		}

		if (f < -15 || f > 15) {
			g_error("Replaygain missing preamp \"%s\" is not between -15 and"
				"15 at line %i\n", param->value, param->line);
		}

		replay_gain_missing_preamp = pow(10, f / 20.0);
	}
}
