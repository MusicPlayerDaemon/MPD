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

/* 
 * Media MVP audio output based on code from MVPMC project:
 * http://mvpmc.sourceforge.net/
 */

#include "config.h"
#include "output_api.h"
#include "fd_util.h"

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

static const unsigned mvp_sample_rates[][3] = {
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

/**
 * The quark used for GError.domain.
 */
static inline GQuark
mvp_output_quark(void)
{
	return g_quark_from_static_string("mvp_output");
}

/**
 * Translate a sample rate to a MVP sample rate.
 *
 * @param sample_rate the sample rate in Hz
 */
static unsigned
mvp_find_sample_rate(unsigned sample_rate)
{
	for (unsigned i = 0; i < G_N_ELEMENTS(mvp_sample_rates); ++i)
		if (mvp_sample_rates[i][1] == sample_rate)
			return mvp_sample_rates[i][0];

	return (unsigned)-1;
}

static bool
mvp_output_test_default_device(void)
{
	int fd;

	fd = open_cloexec("/dev/adec_pcm", O_WRONLY, 0);

	if (fd >= 0) {
		close(fd);
		return true;
	}

	g_warning("Error opening PCM device \"/dev/adec_pcm\": %s\n",
		  strerror(errno));

	return false;
}

static void *
mvp_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		G_GNUC_UNUSED const struct config_param *param,
		G_GNUC_UNUSED GError **error)
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

static bool
mvp_set_pcm_params(struct mvp_data *md, struct audio_format *audio_format,
		   GError **error)
{
	unsigned mix[5];

	switch (audio_format->channels) {
	case 1:
		mix[0] = 1;
		break;

	case 2:
		mix[0] = 0;
		break;

	default:
		g_debug("unsupported channel count %u - falling back to stereo",
			audio_format->channels);
		audio_format->channels = 2;
		mix[0] = 0;
		break;
	}

	/* 0,1=24bit(24) , 2,3=16bit */
	switch (audio_format->bits) {
	case 16:
		mix[1] = 2;
		break;

	case 24:
		mix[1] = 0;
		break;

	default:
		g_debug("unsupported sample format %u - falling back to stereo",
			audio_format->bits);
		audio_format->bits = 16;
		mix[1] = 2;
		break;
	}

	mix[3] = 0;	/* stream type? */
	mix[4] = G_BYTE_ORDER == G_LITTLE_ENDIAN;

	/*
	 * if there is an exact match for the frequency, use it.
	 */
	mix[2] = mvp_find_sample_rate(audio_format->sample_rate);
	if (mix[2] == (unsigned)-1) {
		g_set_error(error, mvp_output_quark(), 0,
			    "Can not find suitable output frequency for %u",
			    audio_format->sample_rate);
		return false;
	}

	if (ioctl(md->fd, MVP_SET_AUD_FORMAT, &mix) < 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Can not set audio format");
		return false;
	}

	if (ioctl(md->fd, MVP_SET_AUD_SYNC, 2) != 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Can not set audio sync");
		return false;
	}

	if (ioctl(md->fd, MVP_SET_AUD_PLAY, 0) < 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Can not set audio play mode");
		return false;
	}

	return true;
}

static bool
mvp_output_open(void *data, struct audio_format *audio_format, GError **error)
{
	struct mvp_data *md = data;
	long long int stc = 0;
	int mix[5] = { 0, 2, 7, 1, 0 };
	bool success;

	md->fd = open_cloexec("/dev/adec_pcm", O_RDWR | O_NONBLOCK, 0);
	if (md->fd < 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Error opening /dev/adec_pcm: %s",
			    strerror(errno));
		return false;
	}
	if (ioctl(md->fd, MVP_SET_AUD_SRC, 1) < 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Error setting audio source: %s",
			    strerror(errno));
		return false;
	}
	if (ioctl(md->fd, MVP_SET_AUD_STREAMTYPE, 0) < 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Error setting audio streamtype: %s",
			    strerror(errno));
		return false;
	}
	if (ioctl(md->fd, MVP_SET_AUD_FORMAT, &mix) < 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Error setting audio format: %s",
			    strerror(errno));
		return false;
	}
	ioctl(md->fd, MVP_SET_AUD_STC, &stc);
	if (ioctl(md->fd, MVP_SET_AUD_BYPASS, 1) < 0) {
		g_set_error(error, mvp_output_quark(), errno,
			    "Error setting audio streamtype: %s",
			    strerror(errno));
		return false;
	}

	success = mvp_set_pcm_params(md, audio_format, error);
	if (!success)
		return false;

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
mvp_output_play(void *data, const void *chunk, size_t size, GError **error)
{
	struct mvp_data *md = data;
	ssize_t ret;

	/* reopen the device since it was closed by dropBufferedAudio */
	if (md->fd < 0) {
		bool success;

		success = mvp_output_open(md, &md->audio_format, error);
		if (!success)
			return 0;
	}

	while (true) {
		ret = write(md->fd, chunk, size);
		if (ret > 0)
			return (size_t)ret;

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			g_set_error(error, mvp_output_quark(), errno,
				    "Failed to write: %s", strerror(errno));
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
