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

#include "../audioOutput.h"

#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>

typedef struct _NullData {
	uint64_t nextPlay;
	int rate;
} NullData;

static NullData *newNullData(void)
{
	NullData *ret;

	ret = xmalloc(sizeof(NullData));
	ret->nextPlay = 0;
	ret->rate = 0;

	return ret;
}

static void freeNullData(NullData *nd)
{
	free(nd);
}

static uint64_t null_getTime(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return ((uint64_t)tv.tv_sec * 1000000) + tv.tv_usec;
}

static int null_initDriver(AudioOutput *audioOutput, ConfigParam *param)
{
	NullData *nd;

	nd = newNullData();
	audioOutput->data = nd;

	return 0;
}

static void null_finishDriver(AudioOutput *audioOutput)
{
	freeNullData((NullData *)audioOutput->data);
}

static int null_openDevice(AudioOutput *audioOutput)
{
	NullData *nd;
	AudioFormat *af;

	nd = audioOutput->data;
	af = &audioOutput->outAudioFormat;
	nd->rate = af->sampleRate * (af->bits / CHAR_BIT) * af->channels;
	audioOutput->open = 1;

	return 0;
}

static void null_dropBufferedAudio(AudioOutput *audioOutput)
{
	NullData *nd;

	nd = audioOutput->data;
	nd->nextPlay = 0;
}

static void null_closeDevice(AudioOutput *audioOutput)
{
	NullData *nd;

	nd = audioOutput->data;
	nd->nextPlay = 0;
	nd->rate = 0;
	audioOutput->open = 0;
}

static int null_playAudio(AudioOutput *audioOutput, char *playChunk, int size)
{
	NullData *nd;
	uint64_t now;

	nd = audioOutput->data;
	now = null_getTime();

	if (nd->nextPlay == 0)
		nd->nextPlay = now;
	else if (nd->nextPlay > now)
		my_usleep(nd->nextPlay - now);

	nd->nextPlay += ((uint64_t)size * 1000000) / nd->rate;

	return 0;
}

AudioOutputPlugin nullPlugin = {
	"null",
	NULL,
	null_initDriver,
	null_finishDriver,
	null_openDevice,
	null_playAudio,
	null_dropBufferedAudio,
	null_closeDevice,
	NULL,
};
