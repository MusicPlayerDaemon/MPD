/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * OSS audio output (c) 2004, 2005, 2006, 2007 by Eric Wong <eric@petta-tech.com>
 *                   and Warren Dukes <warren.dukes@gmail.com>
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

#ifdef HAVE_OSS

#include "../conf.h"
#include "../log.h"

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <soundcard.h>
#else /* !(defined(__OpenBSD__) || defined(__NetBSD__) */
# include <sys/soundcard.h>
#endif /* !(defined(__OpenBSD__) || defined(__NetBSD__) */

#ifdef WORDS_BIGENDIAN
# define	AFMT_S16_MPD	 AFMT_S16_BE
#else
# define	AFMT_S16_MPD	 AFMT_S16_LE
#endif /* WORDS_BIGENDIAN */

typedef struct _OssData {
	int fd;
	const char *device;
	int channels;
	int sampleRate;
	int bitFormat;
	int bits;
	int *supported[3];
	int numSupported[3];
	int *unsupported[3];
	int numUnsupported[3];
} OssData;

#define OSS_SUPPORTED		1
#define OSS_UNSUPPORTED		0
#define OSS_UNKNOWN		-1

#define OSS_RATE		0
#define OSS_CHANNELS		1
#define OSS_BITS		2

static int getIndexForParam(int param)
{
	int index = 0;

	switch (param) {
	case SNDCTL_DSP_SPEED:
		index = OSS_RATE;
		break;
	case SNDCTL_DSP_CHANNELS:
		index = OSS_CHANNELS;
		break;
	case SNDCTL_DSP_SAMPLESIZE:
		index = OSS_BITS;
		break;
	}

	return index;
}

static int findSupportedParam(OssData * od, int param, int val)
{
	int i;
	int index = getIndexForParam(param);

	for (i = 0; i < od->numSupported[index]; i++) {
		if (od->supported[index][i] == val)
			return 1;
	}

	return 0;
}

static int canConvert(int index, int val)
{
	switch (index) {
	case OSS_BITS:
		if (val != 16)
			return 0;
		break;
	case OSS_CHANNELS:
		if (val != 2)
			return 0;
		break;
	}

	return 1;
}

static int getSupportedParam(OssData * od, int param, int val)
{
	int i;
	int index = getIndexForParam(param);
	int ret = -1;
	int least = val;
	int diff;

	for (i = 0; i < od->numSupported[index]; i++) {
		diff = od->supported[index][i] - val;
		if (diff < 0)
			diff = -diff;
		if (diff < least) {
			if (!canConvert(index, od->supported[index][i])) {
				continue;
			}
			least = diff;
			ret = od->supported[index][i];
		}
	}

	return ret;
}

static int findUnsupportedParam(OssData * od, int param, int val)
{
	int i;
	int index = getIndexForParam(param);

	for (i = 0; i < od->numUnsupported[index]; i++) {
		if (od->unsupported[index][i] == val)
			return 1;
	}

	return 0;
}

static void addSupportedParam(OssData * od, int param, int val)
{
	int index = getIndexForParam(param);

	od->numSupported[index]++;
	od->supported[index] = xrealloc(od->supported[index],
				       od->numSupported[index] * sizeof(int));
	od->supported[index][od->numSupported[index] - 1] = val;
}

static void addUnsupportedParam(OssData * od, int param, int val)
{
	int index = getIndexForParam(param);

	od->numUnsupported[index]++;
	od->unsupported[index] = xrealloc(od->unsupported[index],
					 od->numUnsupported[index] *
					 sizeof(int));
	od->unsupported[index][od->numUnsupported[index] - 1] = val;
}

static void removeSupportedParam(OssData * od, int param, int val)
{
	int i = 0;
	int j = 0;
	int index = getIndexForParam(param);

	for (i = 0; i < od->numSupported[index] - 1; i++) {
		if (od->supported[index][i] == val)
			j = 1;
		od->supported[index][i] = od->supported[index][i + j];
	}

	od->numSupported[index]--;
	od->supported[index] = xrealloc(od->supported[index],
				       od->numSupported[index] * sizeof(int));
}

static void removeUnsupportedParam(OssData * od, int param, int val)
{
	int i = 0;
	int j = 0;
	int index = getIndexForParam(param);

	for (i = 0; i < od->numUnsupported[index] - 1; i++) {
		if (od->unsupported[index][i] == val)
			j = 1;
		od->unsupported[index][i] = od->unsupported[index][i + j];
	}

	od->numUnsupported[index]--;
	od->unsupported[index] = xrealloc(od->unsupported[index],
					 od->numUnsupported[index] *
					 sizeof(int));
}

static int isSupportedParam(OssData * od, int param, int val)
{
	if (findSupportedParam(od, param, val))
		return OSS_SUPPORTED;
	if (findUnsupportedParam(od, param, val))
		return OSS_UNSUPPORTED;
	return OSS_UNKNOWN;
}

static void supportParam(OssData * od, int param, int val)
{
	int supported = isSupportedParam(od, param, val);

	if (supported == OSS_SUPPORTED)
		return;

	if (supported == OSS_UNSUPPORTED) {
		removeUnsupportedParam(od, param, val);
	}

	addSupportedParam(od, param, val);
}

static void unsupportParam(OssData * od, int param, int val)
{
	int supported = isSupportedParam(od, param, val);

	if (supported == OSS_UNSUPPORTED)
		return;

	if (supported == OSS_SUPPORTED) {
		removeSupportedParam(od, param, val);
	}

	addUnsupportedParam(od, param, val);
}

static OssData *newOssData(void)
{
	OssData *ret = xmalloc(sizeof(OssData));

	ret->device = NULL;
	ret->fd = -1;

	ret->supported[OSS_RATE] = NULL;
	ret->supported[OSS_CHANNELS] = NULL;
	ret->supported[OSS_BITS] = NULL;
	ret->unsupported[OSS_RATE] = NULL;
	ret->unsupported[OSS_CHANNELS] = NULL;
	ret->unsupported[OSS_BITS] = NULL;

	ret->numSupported[OSS_RATE] = 0;
	ret->numSupported[OSS_CHANNELS] = 0;
	ret->numSupported[OSS_BITS] = 0;
	ret->numUnsupported[OSS_RATE] = 0;
	ret->numUnsupported[OSS_CHANNELS] = 0;
	ret->numUnsupported[OSS_BITS] = 0;

	supportParam(ret, SNDCTL_DSP_SPEED, 48000);
	supportParam(ret, SNDCTL_DSP_SPEED, 44100);
	supportParam(ret, SNDCTL_DSP_CHANNELS, 2);
	supportParam(ret, SNDCTL_DSP_SAMPLESIZE, 16);

	return ret;
}

static void freeOssData(OssData * od)
{
	if (od->supported[OSS_RATE])
		free(od->supported[OSS_RATE]);
	if (od->supported[OSS_CHANNELS])
		free(od->supported[OSS_CHANNELS]);
	if (od->supported[OSS_BITS])
		free(od->supported[OSS_BITS]);
	if (od->unsupported[OSS_RATE])
		free(od->unsupported[OSS_RATE]);
	if (od->unsupported[OSS_CHANNELS])
		free(od->unsupported[OSS_CHANNELS]);
	if (od->unsupported[OSS_BITS])
		free(od->unsupported[OSS_BITS]);

	free(od);
}

#define OSS_STAT_NO_ERROR 	0
#define OSS_STAT_NOT_CHAR_DEV	-1
#define OSS_STAT_NO_PERMS	-2
#define OSS_STAT_DOESN_T_EXIST	-3
#define OSS_STAT_OTHER		-4

static int oss_statDevice(const char *device, int *stErrno)
{
	struct stat st;

	if (0 == stat(device, &st)) {
		if (!S_ISCHR(st.st_mode)) {
			return OSS_STAT_NOT_CHAR_DEV;
		}
	} else {
		*stErrno = errno;

		switch (errno) {
		case ENOENT:
		case ENOTDIR:
			return OSS_STAT_DOESN_T_EXIST;
		case EACCES:
			return OSS_STAT_NO_PERMS;
		default:
			return OSS_STAT_OTHER;
		}
	}

	return 0;
}

static const char *default_devices[] = { "/dev/sound/dsp", "/dev/dsp" };

static int oss_testDefault(void)
{
	int fd, i;

	for (i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		if ((fd = open(default_devices[i], O_WRONLY)) >= 0) {
			xclose(fd);
			return 0;
		}
		WARNING("Error opening OSS device \"%s\": %s\n",
		        default_devices[i], strerror(errno));
	}

	return -1;
}

static int oss_open_default(AudioOutput *ao, ConfigParam *param, OssData *od)
{
	int i;
	int err[ARRAY_SIZE(default_devices)];
	int ret[ARRAY_SIZE(default_devices)];

	for (i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		ret[i] = oss_statDevice(default_devices[i], &err[i]);
		if (ret[i] == 0) {
			od->device = default_devices[i];
			return 0;
		}
	}

	if (param)
		ERROR("error trying to open specified OSS device"
	              " at line %i\n", param->line);
	else
		ERROR("error trying to open default OSS device\n");

	for (i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		const char *dev = default_devices[i];
		switch(ret[i]) {
		case OSS_STAT_DOESN_T_EXIST:
			ERROR("%s not found\n", dev);
			break;
		case OSS_STAT_NOT_CHAR_DEV:
			ERROR("%s is not a character device\n", dev);
			break;
		case OSS_STAT_NO_PERMS:
			ERROR("%s: permission denied\n", dev);
			break;
		default:
			ERROR("Error accessing %s: %s", dev, strerror(err[i]));
		}
	}
	exit(EXIT_FAILURE);
	return 0; /* some compilers can be dumb... */
}

static int oss_initDriver(AudioOutput * audioOutput, ConfigParam * param)
{
	OssData *od = newOssData();
	audioOutput->data = od;
	if (param) {
		BlockParam *bp = getBlockParam(param, "device");
		if (bp) {
			od->device = bp->value;
			return 0;
		}
	}
	return oss_open_default(audioOutput, param, od);
}

static void oss_finishDriver(AudioOutput * audioOutput)
{
	OssData *od = audioOutput->data;

	freeOssData(od);
}

static int setParam(OssData * od, int param, int *value)
{
	int val = *value;
	int copy;
	int supported = isSupportedParam(od, param, val);

	do {
		if (supported == OSS_UNSUPPORTED) {
			val = getSupportedParam(od, param, val);
			if (copy < 0)
				return -1;
		}
		copy = val;
		if (ioctl(od->fd, param, &copy)) {
			unsupportParam(od, param, val);
			supported = OSS_UNSUPPORTED;
		} else {
			if (supported == OSS_UNKNOWN) {
				supportParam(od, param, val);
				supported = OSS_SUPPORTED;
			}
			val = copy;
		}
	} while (supported == OSS_UNSUPPORTED);

	*value = val;

	return 0;
}

static void oss_close(OssData * od)
{
	if (od->fd >= 0)
		while (close(od->fd) && errno == EINTR) ;
	od->fd = -1;
}

static int oss_open(AudioOutput * audioOutput)
{
	int tmp;
	OssData *od = audioOutput->data;

	if ((od->fd = open(od->device, O_WRONLY)) < 0) {
		ERROR("Error opening OSS device \"%s\": %s\n", od->device,
		      strerror(errno));
		goto fail;
	}

	if (setParam(od, SNDCTL_DSP_CHANNELS, &od->channels)) {
		ERROR("OSS device \"%s\" does not support %i channels: %s\n",
		      od->device, od->channels, strerror(errno));
		goto fail;
	}

	if (setParam(od, SNDCTL_DSP_SPEED, &od->sampleRate)) {
		ERROR("OSS device \"%s\" does not support %i Hz audio: %s\n",
		      od->device, od->sampleRate, strerror(errno));
		goto fail;
	}

	switch (od->bits) {
	case 8:
		tmp = AFMT_S8;
		break;
	case 16:
		tmp = AFMT_S16_MPD;
	}

	if (setParam(od, SNDCTL_DSP_SAMPLESIZE, &tmp)) {
		ERROR("OSS device \"%s\" does not support %i bit audio: %s\n",
		      od->device, tmp, strerror(errno));
		goto fail;
	}

	audioOutput->open = 1;

	return 0;

fail:
	oss_close(od);
	audioOutput->open = 0;
	return -1;
}

static int oss_openDevice(AudioOutput * audioOutput)
{
	int ret = -1;
	OssData *od = audioOutput->data;
	AudioFormat *audioFormat = &audioOutput->outAudioFormat;

	od->channels = audioFormat->channels;
	od->sampleRate = audioFormat->sampleRate;
	od->bits = audioFormat->bits;

	if ((ret = oss_open(audioOutput)) < 0)
		return ret;

	audioFormat->channels = od->channels;
	audioFormat->sampleRate = od->sampleRate;
	audioFormat->bits = od->bits;

	DEBUG("oss device \"%s\" will be playing %i bit %i channel audio at "
	      "%i Hz\n", od->device, od->bits, od->channels, od->sampleRate);

	return ret;
}

static void oss_closeDevice(AudioOutput * audioOutput)
{
	OssData *od = audioOutput->data;

	oss_close(od);

	audioOutput->open = 0;
}

static void oss_dropBufferedAudio(AudioOutput * audioOutput)
{
	OssData *od = audioOutput->data;

	if (od->fd >= 0) {
		ioctl(od->fd, SNDCTL_DSP_RESET, 0);
		oss_close(od);
	}
}

static int oss_playAudio(AudioOutput * audioOutput, char *playChunk, int size)
{
	OssData *od = audioOutput->data;
	int ret;

	/* reopen the device since it was closed by dropBufferedAudio */
	if (od->fd < 0 && oss_open(audioOutput) < 0)
		return -1;

	while (size > 0) {
		ret = write(od->fd, playChunk, size);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			ERROR("closing oss device \"%s\" due to write error: "
			      "%s\n", od->device, strerror(errno));
			oss_closeDevice(audioOutput);
			return -1;
		}
		playChunk += ret;
		size -= ret;
	}

	return 0;
}

AudioOutputPlugin ossPlugin = {
	"oss",
	oss_testDefault,
	oss_initDriver,
	oss_finishDriver,
	oss_openDevice,
	oss_playAudio,
	oss_dropBufferedAudio,
	oss_closeDevice,
	NULL,	/* sendMetadataFunc */
};

#else /* HAVE OSS */

DISABLED_AUDIO_OUTPUT_PLUGIN(ossPlugin)
#endif /* HAVE_OSS */
