/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * (c)2004 replayGain code by AliasMrJones
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

#include "replay_gain.h"
#include "utils.h"

#include "log.h"
#include "conf.h"
#include "audio_format.h"
#include "os_compat.h"

/* Added 4/14/2004 by AliasMrJones */
int replay_gain_mode = REPLAY_GAIN_OFF;

static float replay_gain_preamp = 1.0;

void replay_gain_global_init(void)
{
	ConfigParam *param = getConfigParam(CONF_REPLAYGAIN);

	if (!param)
		return;

	if (strcmp(param->value, "track") == 0) {
		replay_gain_mode = REPLAY_GAIN_TRACK;
	} else if (strcmp(param->value, "album") == 0) {
		replay_gain_mode = REPLAY_GAIN_ALBUM;
	} else {
		FATAL("replaygain value \"%s\" at line %i is invalid\n",
		      param->value, param->line);
	}

	param = getConfigParam(CONF_REPLAYGAIN_PREAMP);

	if (param) {
		char *test;
		float f = strtod(param->value, &test);

		if (*test != '\0') {
			FATAL("Replaygain preamp \"%s\" is not a number at "
			      "line %i\n", param->value, param->line);
		}

		if (f < -15 || f > 15) {
			FATAL("Replaygain preamp \"%s\" is not between -15 and"
			      "15 at line %i\n", param->value, param->line);
		}

		replay_gain_preamp = pow(10, f / 20.0);
	}
}

static float calc_replay_gain_scale(float gain, float peak)
{
	float scale;

	if (gain == 0.0)
		return (1);
	scale = pow(10.0, gain / 20.0);
	scale *= replay_gain_preamp;
	if (scale > 15.0)
		scale = 15.0;

	if (scale * peak > 1.0) {
		scale = 1.0 / peak;
	}
	return (scale);
}

struct replay_gain_info *replay_gain_info_new(void)
{
	struct replay_gain_info *ret = xmalloc(sizeof(*ret));

	ret->album_gain = 0.0;
	ret->album_peak = 0.0;

	ret->track_gain = 0.0;
	ret->track_peak = 0.0;

	/* set to -1 so that we know in replay_gain_apply to compute the scale */
	ret->scale = -1.0;

	return ret;
}

void replay_gain_info_free(struct replay_gain_info *info)
{
	free(info);
}

void
replay_gain_apply(struct replay_gain_info *info, char *buffer, int size,
		  const struct audio_format *format)
{
	int16_t *buffer16;
	int8_t *buffer8;
	int32_t temp32;
	float scale;

	if (replay_gain_mode == REPLAY_GAIN_OFF || !info)
		return;

	if (info->scale < 0) {
		switch (replay_gain_mode) {
		case REPLAY_GAIN_TRACK:
			DEBUG("computing ReplayGain track scale with gain %f, "
			      "peak %f\n", info->track_gain, info->track_peak);
			info->scale = calc_replay_gain_scale(info->track_gain,
							     info->track_peak);
			break;
		default:
			DEBUG("computing ReplayGain album scale with gain %f, "
			      "peak %f\n", info->album_gain, info->album_peak);
			info->scale = calc_replay_gain_scale(info->album_gain,
							     info->album_peak);
			break;
		}
	}

	if (info->scale <= 1.01 && info->scale >= 0.99)
		return;

	buffer16 = (int16_t *) buffer;
	buffer8 = (int8_t *) buffer;

	scale = info->scale;

	switch (format->bits) {
	case 16:
		while (size > 0) {
			temp32 = *buffer16;
			temp32 *= scale;
			*buffer16 = temp32 > 32767 ? 32767 :
			    (temp32 < -32768 ? -32768 : temp32);
			buffer16++;
			size -= 2;
		}
		break;
	case 8:
		while (size > 0) {
			temp32 = *buffer8;
			temp32 *= scale;
			*buffer8 = temp32 > 127 ? 127 :
			    (temp32 < -128 ? -128 : temp32);
			buffer8++;
			size--;
		}
		break;
	default:
		ERROR("%i bits not supported by doReplaygain!\n", format->bits);
	}
}
