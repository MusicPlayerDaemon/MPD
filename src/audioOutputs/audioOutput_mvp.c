/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * Media MVP audio output based on code from MVPMC project:
 * http://mvpmc.sourceforge.net/
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

#ifdef HAVE_MVP

#include "../conf.h"
#include "../log.h"

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
	unsigned long dsp_status;
	unsigned long stream_decode_type;
	unsigned long sample_rate;
	unsigned long bit_rate;
	unsigned long raw[64 / sizeof(unsigned long)];
} aud_status_t;

#define MVP_SET_AUD_STOP		_IOW('a',1,int)
#define MVP_SET_AUD_PLAY		_IOW('a',2,int)
#define MVP_SET_AUD_PAUSE	_IOW('a',3,int)
#define MVP_SET_AUD_UNPAUSE	_IOW('a',4,int)
#define MVP_SET_AUD_SRC		_IOW('a',5,int)
#define MVP_SET_AUD_MUTE		_IOW('a',6,int)
#define MVP_SET_AUD_BYPASS	_IOW('a',8,int)
#define MVP_SET_AUD_CHANNEL	_IOW('a',9,int)
#define MVP_GET_AUD_STATUS	_IOR('a',10,aud_status_t)
#define MVP_SET_AUD_VOLUME	_IOW('a',13,int)
#define MVP_GET_AUD_VOLUME	_IOR('a',14,int)
#define MVP_SET_AUD_STREAMTYPE	_IOW('a',15,int)
#define MVP_SET_AUD_FORMAT	_IOW('a',16,int)
#define MVP_GET_AUD_SYNC		_IOR('a',21,pts_sync_data_t*)
#define MVP_SET_AUD_STC		_IOW('a',22,long long int *)
#define MVP_SET_AUD_SYNC		_IOW('a',23,int)
#define MVP_SET_AUD_END_STREAM	_IOW('a',25,int)
#define MVP_SET_AUD_RESET	_IOW('a',26,int)
#define MVP_SET_AUD_DAC_CLK	_IOW('a',27,int)
#define MVP_GET_AUD_REGS		_IOW('a',28,aud_ctl_regs_t*)

typedef struct _MvpData {
	int fd;
} MvpData;

static int pcmfrequencies[][3] = {
	{9, 8000, 32000},
	{10, 11025, 44100},
	{11, 12000, 48000},
	{1, 16000, 32000},
	{2, 22050, 44100},
	{3, 24000, 48000},
	{5, 32000, 32000},
	{0, 44100, 44100},
	{7, 48000, 48000},
	{13, 64000, 32000},
	{14, 88200, 44100},
	{15, 96000, 48000}
};

static int numfrequencies = sizeof(pcmfrequencies) / 12;

static int mvp_testDefault(void)
{
	int fd;

	fd = open("/dev/adec_pcm", O_WRONLY);

	if (fd) {
		close(fd);
		return 0;
	}

	WARNING("Error opening PCM device \"/dev/adec_pcm\": %s\n",
		strerror(errno));

	return -1;
}

static int mvp_initDriver(AudioOutput * audioOutput, ConfigParam * param)
{
	MvpData *md = xmalloc(sizeof(MvpData));
	md->fd = -1;
	audioOutput->data = md;

	return 0;
}

static void mvp_finishDriver(AudioOutput * audioOutput)
{
	MvpData *md = audioOutput->data;
	free(md);
}

static int mvp_setPcmParams(MvpData * md, unsigned long rate, int channels,
			    int big_endian, int bits)
{
	int iloop;
	int mix[5];

	if (channels == 1)
		mix[0] = 1;
	else if (channels == 2)
		mix[0] = 0;
	else
		return -1;

	/* 0,1=24bit(24) , 2,3=16bit */
	if (bits == 16)
		mix[1] = 2;
	else if (bits == 24)
		mix[1] = 0;
	else
		return -1;

	mix[3] = 0;	/* stream type? */

	if (big_endian == 1)
		mix[4] = 1;
	else if (big_endian == 0)
		mix[4] = 0;
	else
		return -1;

	/*
	 * if there is an exact match for the frequency, use it.
	 */
	for (iloop = 0; iloop < numfrequencies; iloop++) {
		if (rate == pcmfrequencies[iloop][1]) {
			mix[2] = pcmfrequencies[iloop][0];
			break;
		}
	}

	if (iloop >= numfrequencies) {
		ERROR("Can not find suitable output frequency for %ld\n", rate);
		return -1;
	}

	if (ioctl(md->fd, MVP_SET_AUD_FORMAT, &mix) < 0) {
		ERROR("Can not set audio format\n");
		return -1;
	}

	if (ioctl(md->fd, MVP_SET_AUD_SYNC, 2) != 0) {
		ERROR("Can not set audio sync\n");
		return -1;
	}

	if (ioctl(md->fd, MVP_SET_AUD_PLAY, 0) < 0) {
		ERROR("Can not set audio play mode\n");
		return -1;
	}

	return 0;
}

static int mvp_openDevice(AudioOutput * audioOutput)
{
	long long int stc = 0;
	MvpData *md = audioOutput->data;
	AudioFormat *audioFormat = &audioOutput->outAudioFormat;
	int mix[5] = { 0, 2, 7, 1, 0 };

	if ((md->fd = open("/dev/adec_pcm", O_RDWR | O_NONBLOCK)) < 0) {
		ERROR("Error opening /dev/adec_pcm: %s\n", strerror(errno));
		return -1;
	}
	if (ioctl(md->fd, MVP_SET_AUD_SRC, 1) < 0) {
		ERROR("Error setting audio source: %s\n", strerror(errno));
		return -1;
	}
	if (ioctl(md->fd, MVP_SET_AUD_STREAMTYPE, 0) < 0) {
		ERROR("Error setting audio streamtype: %s\n", strerror(errno));
		return -1;
	}
	if (ioctl(md->fd, MVP_SET_AUD_FORMAT, &mix) < 0) {
		ERROR("Error setting audio format: %s\n", strerror(errno));
		return -1;
	}
	ioctl(md->fd, MVP_SET_AUD_STC, &stc);
	if (ioctl(md->fd, MVP_SET_AUD_BYPASS, 1) < 0) {
		ERROR("Error setting audio streamtype: %s\n", strerror(errno));
		return -1;
	}
#ifdef WORDS_BIGENDIAN
	mvp_setPcmParams(md, audioFormat->sampleRate, audioFormat->channels, 0,
			 audioFormat->bits);
#else
	mvp_setPcmParams(md, audioFormat->sampleRate, audioFormat->channels, 1,
			 audioFormat->bits);
#endif
	audioOutput->open = 1;
	return 0;
}

static void mvp_closeDevice(AudioOutput * audioOutput)
{
	MvpData *md = audioOutput->data;
	if (md->fd >= 0)
		close(md->fd);
	md->fd = -1;
	audioOutput->open = 0;
}

static void mvp_dropBufferedAudio(AudioOutput * audioOutput)
{
	MvpData *md = audioOutput->data;
	if (md->fd >= 0) {
		ioctl(md->fd, MVP_SET_AUD_RESET, 0x11);
		close(md->fd);
		md->fd = -1;
		audioOutput->open = 0;
	}
}

static int mvp_playAudio(AudioOutput * audioOutput, char *playChunk, int size)
{
	MvpData *md = audioOutput->data;
	int ret;

	/* reopen the device since it was closed by dropBufferedAudio */
	if (md->fd < 0)
		mvp_openDevice(audioOutput);

	while (size > 0) {
		ret = write(md->fd, playChunk, size);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			ERROR("closing mvp PCM device due to write error: "
			      "%s\n", strerror(errno));
			mvp_closeDevice(audioOutput);
			return -1;
		}
		playChunk += ret;
		size -= ret;
	}
	return 0;
}

AudioOutputPlugin mvpPlugin = {
	"mvp",
	mvp_testDefault,
	mvp_initDriver,
	mvp_finishDriver,
	mvp_openDevice,
	mvp_playAudio,
	mvp_dropBufferedAudio,
	mvp_closeDevice,
	NULL,	/* sendMetadataFunc */
};

#else /* HAVE_MVP */

DISABLED_AUDIO_OUTPUT_PLUGIN(mvpPlugin)
#endif /* HAVE_MVP */
