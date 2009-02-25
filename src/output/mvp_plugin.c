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

#include "../output_api.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mvp"

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

struct mvp_data {
	struct audio_format audio_format;
	int fd;
};

static unsigned mvp_sample_rates[][3] = {
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

static bool
mvp_output_test_default_device(void)
{
	int fd;

	fd = open("/dev/adec_pcm", O_WRONLY);

	if (fd) {
		close(fd);
		return true;
	}

	g_warning("Error opening PCM device \"/dev/adec_pcm\": %s\n",
		  strerror(errno));

	return false;
}

static void *
mvp_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		G_GNUC_UNUSED const struct config_param *param)
{
	struct mvp_data *md = g_new(struct mvp_data, 1);
	md->fd = -1;

	return md;
}

static void
mvp_output_finish(void *data)
{
	struct mvp_data *md = data;
	g_free(md);
}

static int
mvp_set_pcm_params(struct mvp_data *md, unsigned long rate, int channels,
		   unsigned bits)
{
	unsigned iloop;
	unsigned mix[5];

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
	mix[4] = G_BYTE_ORDER == G_LITTLE_ENDIAN;

	/*
	 * if there is an exact match for the frequency, use it.
	 */
	for (iloop = 0; iloop < G_N_ELEMENTS(mvp_sample_rates); iloop++) {
		if (rate == mvp_sample_rates[iloop][1]) {
			mix[2] = mvp_sample_rates[iloop][0];
			break;
		}
	}

	if (iloop >= G_N_ELEMENTS(mvp_sample_rates)) {
		g_warning("Can not find suitable output frequency for %ld\n",
			  rate);
		return -1;
	}

	if (ioctl(md->fd, MVP_SET_AUD_FORMAT, &mix) < 0) {
		g_warning("Can not set audio format\n");
		return -1;
	}

	if (ioctl(md->fd, MVP_SET_AUD_SYNC, 2) != 0) {
		g_warning("Can not set audio sync\n");
		return -1;
	}

	if (ioctl(md->fd, MVP_SET_AUD_PLAY, 0) < 0) {
		g_warning("Can not set audio play mode\n");
		return -1;
	}

	return 0;
}

static bool
mvp_output_open(void *data, struct audio_format *audio_format)
{
	struct mvp_data *md = data;
	long long int stc = 0;
	int mix[5] = { 0, 2, 7, 1, 0 };

	if ((md->fd = open("/dev/adec_pcm", O_RDWR | O_NONBLOCK)) < 0) {
		g_warning("Error opening /dev/adec_pcm: %s\n",
			  strerror(errno));
		return false;
	}
	if (ioctl(md->fd, MVP_SET_AUD_SRC, 1) < 0) {
		g_warning("Error setting audio source: %s\n",
			  strerror(errno));
		return false;
	}
	if (ioctl(md->fd, MVP_SET_AUD_STREAMTYPE, 0) < 0) {
		g_warning("Error setting audio streamtype: %s\n",
			  strerror(errno));
		return false;
	}
	if (ioctl(md->fd, MVP_SET_AUD_FORMAT, &mix) < 0) {
		g_warning("Error setting audio format: %s\n",
			  strerror(errno));
		return false;
	}
	ioctl(md->fd, MVP_SET_AUD_STC, &stc);
	if (ioctl(md->fd, MVP_SET_AUD_BYPASS, 1) < 0) {
		g_warning("Error setting audio streamtype: %s\n",
			  strerror(errno));
		return false;
	}
	mvp_set_pcm_params(md, audio_format->sample_rate,
			   audio_format->channels, audio_format->bits);
	md->audio_format = *audio_format;
	return true;
}

static void mvp_output_close(void *data)
{
	struct mvp_data *md = data;
	if (md->fd >= 0)
		close(md->fd);
	md->fd = -1;
}

static void mvp_output_cancel(void *data)
{
	struct mvp_data *md = data;
	if (md->fd >= 0) {
		ioctl(md->fd, MVP_SET_AUD_RESET, 0x11);
		close(md->fd);
		md->fd = -1;
	}
}

static size_t
mvp_output_play(void *data, const void *chunk, size_t size)
{
	struct mvp_data *md = data;
	ssize_t ret;

	/* reopen the device since it was closed by dropBufferedAudio */
	if (md->fd < 0)
		mvp_output_open(md, &md->audio_format);

	while (true) {
		ret = write(md->fd, chunk, size);
		if (ret > 0)
			return (size_t)ret;

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			g_warning("closing mvp PCM device due to write error: "
				  "%s\n", strerror(errno));
			return 0;
		}
	}
}

const struct audio_output_plugin mvp_output_plugin = {
	.name = "mvp",
	.test_default_device = mvp_output_test_default_device,
	.init = mvp_output_init,
	.finish = mvp_output_finish,
	.open = mvp_output_open,
	.close = mvp_output_close,
	.play = mvp_output_play,
	.cancel = mvp_output_cancel,
};
