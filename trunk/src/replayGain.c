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

#include "replayGain.h"
#include "utils.h"

#include "log.h"
#include "conf.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

/* Added 4/14/2004 by AliasMrJones */
int replayGainState = REPLAYGAIN_OFF;

static float replayGainPreamp = 1.0;

void initReplayGainState(void)
{
	ConfigParam *param = getConfigParam(CONF_REPLAYGAIN);

	if (!param)
		return;

	if (strcmp(param->value, "track") == 0) {
		replayGainState = REPLAYGAIN_TRACK;
	} else if (strcmp(param->value, "album") == 0) {
		replayGainState = REPLAYGAIN_ALBUM;
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

		replayGainPreamp = pow(10, f / 20.0);
	}
}

static float computeReplayGainScale(float gain, float peak)
{
	float scale;

	if (gain == 0.0)
		return (1);
	scale = pow(10.0, gain / 20.0);
	scale *= replayGainPreamp;
	if (scale > 15.0)
		scale = 15.0;

	if (scale * peak > 1.0) {
		scale = 1.0 / peak;
	}
	return (scale);
}

ReplayGainInfo *newReplayGainInfo(void)
{
	ReplayGainInfo *ret = xmalloc(sizeof(ReplayGainInfo));

	ret->albumGain = 0.0;
	ret->albumPeak = 0.0;

	ret->trackGain = 0.0;
	ret->trackPeak = 0.0;

	/* set to -1 so that we know in doReplayGain to compute the scale */
	ret->scale = -1.0;

	return ret;
}

void freeReplayGainInfo(ReplayGainInfo * info)
{
	free(info);
}

void doReplayGain(ReplayGainInfo * info, char *buffer, int bufferSize,
		  AudioFormat * format)
{
	mpd_sint16 *buffer16;
	mpd_sint8 *buffer8;
	mpd_sint32 temp32;
	float scale;

	if (replayGainState == REPLAYGAIN_OFF || !info)
		return;

	if (info->scale < 0) {
		switch (replayGainState) {
		case REPLAYGAIN_TRACK:
			info->scale = computeReplayGainScale(info->trackGain,
							     info->trackPeak);
			break;
		default:
			info->scale = computeReplayGainScale(info->albumGain,
							     info->albumPeak);
			break;
		}
	}

	if (info->scale <= 1.01 && info->scale >= 0.99)
		return;

	buffer16 = (mpd_sint16 *) buffer;
	buffer8 = (mpd_sint8 *) buffer;

	scale = info->scale;

	switch (format->bits) {
	case 16:
		while (bufferSize > 0) {
			temp32 = *buffer16;
			temp32 *= scale;
			*buffer16 = temp32 > 32767 ? 32767 :
			    (temp32 < -32768 ? -32768 : temp32);
			buffer16++;
			bufferSize -= 2;
		}
		break;
	case 8:
		while (bufferSize > 0) {
			temp32 = *buffer8;
			temp32 *= scale;
			*buffer8 = temp32 > 127 ? 127 :
			    (temp32 < -128 ? -128 : temp32);
			buffer8++;
			bufferSize--;
		}
		break;
	default:
		ERROR("%i bits not supported by doReplaygain!\n", format->bits);
	}
}
