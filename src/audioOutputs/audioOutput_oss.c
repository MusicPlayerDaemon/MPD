/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
 * This project's homepage is: http://www.musicpd.org
 *
 * OSS audio output (c) 2004 by Eric Wong <eric@petta-tech.com>
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
#include "../sig_handlers.h"

#include <string.h>
#include <assert.h>
#include <signal.h>

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

typedef struct _OssData {
	int fd;
	char * device;
	int channels;
	int sampleRate;
	int bitFormat;
	int bits;
} OssData;

static OssData * newOssData() {
	OssData * ret = malloc(sizeof(OssData));

	ret->device = NULL;
	ret->fd = -1;

	return ret;
}

static void freeOssData(OssData * od) {
	if(od->device) free(od->device);

	free(od);
}

#define OSS_STAT_NO_ERROR 	0
#define OSS_STAT_NOT_CHAR_DEV	-1
#define OSS_STAT_NO_PERMS	-2
#define OSS_STAT_DOESN_T_EXIST	-3
#define OSS_STAT_OTHER		-4

static int oss_statDevice(char * device, int * stErrno) {
	struct stat st;
	
	if(0 == stat(device, &st)) {
		if(!S_ISCHR(st.st_mode)) {
			return OSS_STAT_NOT_CHAR_DEV;
		}
	}
	else {
		*stErrno = errno;

		switch(errno) {
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

static int oss_initDriver(AudioOutput * audioOutput, ConfigParam * param) {
	BlockParam * bp = getBlockParam(param, "device");
	OssData * od = newOssData();
	
	audioOutput->data = od;

	if(!bp) {
		int err[2];
		int ret[2];
		
		ret[0] = oss_statDevice("/dev/sound/dsp", err);
		ret[1] = oss_statDevice("/dev/dsp", err+1);

		if(ret[0] == 0) od->device = strdup("/dev/sound/dsp");
		else if(ret[1] == 0) od->device = strdup("/dev/dsp");
		else {
			ERROR("Error trying to open default OSS device "
				"specified at line %i\n", param->line);

			if(ret[0] == ret[1] == OSS_STAT_DOESN_T_EXIST) {
				ERROR("Neither /dev/dsp nor /dev/sound/dsp "
						"were found\n");
			}
			else if(ret[0] == OSS_STAT_NOT_CHAR_DEV) {
				ERROR("/dev/sound/dsp is not a char device");
			}
			else if(ret[1] == OSS_STAT_NOT_CHAR_DEV) {
				ERROR("/dev/dsp is not a char device");
			}
			else if(ret[0] == OSS_STAT_NO_PERMS) {
				ERROR("no permission to access /dev/sound/dsp");
			}
			else if(ret[1] == OSS_STAT_NO_PERMS) {
				ERROR("no permission to access /dev/dsp");
			}
			else if(ret[0] == OSS_STAT_OTHER) {
				ERROR("Error accessing /dev/sound/dsp: %s",
						strerror(err[0]));
			}
			else if(ret[1] == OSS_STAT_OTHER) {
				ERROR("Error accessing /dev/dsp: %s",
						strerror(err[1]));
			}
			
			exit(EXIT_FAILURE);
		}
	}
	else od->device = strdup(bp->value);

	return 0;
}

static void oss_finishDriver(AudioOutput * audioOutput) {
	OssData * od = audioOutput->data;

	freeOssData(od);
}

static int oss_open(AudioOutput * audioOutput) {
	OssData * od = audioOutput->data;

	if((od->fd = open(od->device, O_WRONLY)) < 0) {
		ERROR("Error opening OSS device \"%s\": %s\n", od->device, 
				strerror(errno));
		goto fail;
	}

	if(ioctl(od->fd, SNDCTL_DSP_SETFMT, &od->bitFormat)) {
		ERROR("Error setting bitformat on OSS device \"%s\": %s\n", 
				od->device, 
				strerror(errno));
		goto fail;
	}

	if(ioctl(od->fd, SNDCTL_DSP_CHANNELS, &od->channels)) {
		ERROR("OSS device \"%s\" does not support %i channels: %s\n", 
				od->device,
				od->channels,
				strerror(errno));
		goto fail;
	}

	if(ioctl(od->fd, SNDCTL_DSP_SPEED, &od->sampleRate)) {
		ERROR("OSS device \"%s\" does not support %i Hz audio: %s\n", 
				od->device,
				od->sampleRate,
				strerror(errno));
		goto fail;
	}

	if(ioctl(od->fd, SNDCTL_DSP_SAMPLESIZE, &od->bits)) {
		ERROR("OSS device \"%s\" does not support %i bit audio: %s\n", 
				od->device,
				od->bits,
				strerror(errno));
		goto fail;
	}

	audioOutput->open = 1;

	return 0;

fail:
	if(od->fd >= 0) close(od->fd);
	audioOutput->open = 0;
	return -1;
}

static int oss_openDevice(AudioOutput * audioOutput) 
{
	OssData * od = audioOutput->data;
	AudioFormat * audioFormat = &audioOutput->outAudioFormat;
#ifdef WORDS_BIGENDIAN
	od->bitFormat = AFMT_S16_BE;
#else
	od->bitFormat = AFMT_S16_LE;
#endif
	od->channels = audioFormat->channels;	
	od->sampleRate = audioFormat->sampleRate;
	od->bits = audioFormat->bits;

	return oss_open(audioOutput);
}

static void oss_closeDevice(AudioOutput * audioOutput) {
	OssData * od = audioOutput->data;

	if(od->fd >= 0) {
		close(od->fd);
		od->fd = -1;
	}

	audioOutput->open = 0;
}

static void oss_dropBufferedAudio(AudioOutput * audioOutput) {
	OssData * od = audioOutput->data;

	if(od->fd >= 0) {
		ioctl(od->fd, SNDCTL_DSP_RESET, 0);
		oss_closeDevice(audioOutput);
	}

	/*oss_open(audioOutput);*/
}

static int oss_playAudio(AudioOutput * audioOutput, char * playChunk, 
		int size) 
{
	OssData * od = audioOutput->data;
	int ret;

	while (size > 0) {
		ret = write(od->fd, playChunk, size);
		if(ret<0) {
			ERROR("closing audio device due to write error\n");
			oss_closeDevice(audioOutput);
			return -1;
		}
		playChunk += ret;
		size -= ret;
	}

	return 0;
}

AudioOutputPlugin ossPlugin =
{
	"oss",
	oss_initDriver,
	oss_finishDriver,
	oss_openDevice,
	oss_playAudio,
	oss_dropBufferedAudio,
	oss_closeDevice,
	NULL /* sendMetadataFunc */
};

#else /* HAVE OSS */

AudioOutputPlugin ossPlugin =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL /* sendMetadataFunc */
};

#endif /* HAVE_OSS */


