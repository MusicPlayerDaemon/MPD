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

#include "../output_api.h"
#include "../timer.h"

static int null_initDriver(AudioOutput *audioOutput,
			   mpd_unused ConfigParam *param)
{
	audioOutput->data = NULL;
	return 0;
}

static int null_openDevice(AudioOutput *audioOutput)
{
	audioOutput->data = timer_new(&audioOutput->outAudioFormat);
	audioOutput->open = 1;
	return 0;
}

static void null_closeDevice(AudioOutput *audioOutput)
{
	if (audioOutput->data) {
		timer_free(audioOutput->data);
		audioOutput->data = NULL;
	}

	audioOutput->open = 0;
}

static int null_playAudio(AudioOutput *audioOutput,
			  mpd_unused const char *playChunk, size_t size)
{
	Timer *timer = audioOutput->data;

	if (!timer->started)
		timer_start(timer);
	else
		timer_sync(timer);

	timer_add(timer, size);

	return 0;
}

static void null_dropBufferedAudio(AudioOutput *audioOutput)
{
	timer_reset(audioOutput->data);
}

AudioOutputPlugin nullPlugin = {
	"null",
	NULL,
	null_initDriver,
	NULL,
	null_openDevice,
	null_playAudio,
	null_dropBufferedAudio,
	null_closeDevice,
	NULL,
};
