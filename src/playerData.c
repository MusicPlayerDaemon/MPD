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

#include "playerData.h"
#include "conf.h"
#include "log.h"
#include "utils.h"

unsigned int buffered_before_play;

#define DEFAULT_BUFFER_SIZE         2048
#define DEFAULT_BUFFER_BEFORE_PLAY  10

static PlayerData playerData_pd;
PlayerControl pc;

void initPlayerData(void)
{
	float perc = DEFAULT_BUFFER_BEFORE_PLAY;
	char *test;
	int crossfade = 0;
	size_t bufferSize = DEFAULT_BUFFER_SIZE;
	unsigned int buffered_chunks;
	ConfigParam *param;
	size_t device_array_size = audio_device_count() * sizeof(mpd_sint8);

	param = getConfigParam(CONF_AUDIO_BUFFER_SIZE);

	if (param) {
		bufferSize = strtol(param->value, &test, 10);
		if (*test != '\0' || bufferSize <= 0) {
			FATAL("buffer size \"%s\" is not a positive integer, "
			      "line %i\n", param->value, param->line);
		}
	}

	bufferSize *= 1024;

	buffered_chunks = bufferSize / CHUNK_SIZE;

	if (buffered_chunks >= 1 << 15) {
		FATAL("buffer size \"%li\" is too big\n", (long)bufferSize);
	}

	param = getConfigParam(CONF_BUFFER_BEFORE_PLAY);

	if (param) {
		perc = strtod(param->value, &test);
		if (*test != '%' || perc < 0 || perc > 100) {
			FATAL("buffered before play \"%s\" is not a positive "
			      "percentage and less than 100 percent, line %i"
			      "\n", param->value, param->line);
		}
	}

	buffered_before_play = (perc / 100) * buffered_chunks;
	if (buffered_before_play > buffered_chunks) {
		buffered_before_play = buffered_chunks;
	}

	playerData_pd.audioDeviceStates = xmalloc(device_array_size);

	initOutputBuffer(&(playerData_pd.buffer), buffered_chunks);

	notifyInit(&pc.notify);
	pc.error = PLAYER_ERROR_NOERROR;
	pc.state = PLAYER_STATE_STOP;
	pc.queueState = PLAYER_QUEUE_BLANK;
	pc.queueLockState = PLAYER_QUEUE_UNLOCKED;
	pc.crossFade = crossfade;
	pc.softwareVolume = 1000;

	notifyInit(&playerData_pd.decoderControl.notify);
	playerData_pd.decoderControl.stop = 0;
	playerData_pd.decoderControl.start = 0;
	playerData_pd.decoderControl.state = DECODE_STATE_STOP;
	playerData_pd.decoderControl.seek = 0;
	playerData_pd.decoderControl.error = DECODE_ERROR_NOERROR;
	playerData_pd.decoderControl.current_song = NULL;
}

PlayerData *getPlayerData(void)
{
	return &playerData_pd;
}

void freePlayerData(void)
{
	/* We don't want to release this memory until we know our player and
	 * decoder have exited.  Otherwise, their signal handlers will want to
	 * access playerData_pd and we need to keep it available for them */
	waitpid(-1, NULL, 0);

	output_buffer_free(&playerData_pd.buffer);
	free(playerData_pd.audioDeviceStates);
}
