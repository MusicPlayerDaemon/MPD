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

static int oss_initDriver(AudioOutput * audioOutput, ConfigParam * param) {
	BlockParam * bp = getBlockParam(param, "device");
	OssData * od = newOssData();
	
	audioOutput->data = od;

	if(!bp) {
		int fd;

		if(0 <= (fd = open("/dev/sound/dsp", O_WRONLY))) {
			od->device = strdup("/dev/sound/dsp");
			close(fd);
		}
		else if(0 <= (fd = open("/dev/dsp", O_WRONLY))) {
			od->device = strdup("/dev/dsp");
			close(fd);
		}
		else {
			ERROR("Error trying to open default OSS device "
				"specified at line %i\n", param->line);
			ERROR("Specify a OSS device and/or check your "
				"permissions\n");
			exit(EXIT_FAILURE);
		}

		od->fd = -1;

		return 0;
	}

	od->device = strdup(bp->value);

	return 0;
}

static void oss_finishDriver(AudioOutput * audioOutput) {
	OssData * od = audioOutput->data;

	freeOssData(od);
}

static int oss_openDevice(AudioOutput * audioOutput, AudioFormat * audioFormat) 
{
	OssData * od = audioOutput->data;
#ifdef WORDS_BIGENDIAN
	int i = AFMT_S16_BE;
#else
	int i = AFMT_S16_LE;
#endif
	
	if((od->fd = open(od->device, O_WRONLY)) < 0)
		goto fail;
	if(ioctl(od->fd, SNDCTL_DSP_SETFMT, &i))
		goto fail;
	if(ioctl(od->fd, SNDCTL_DSP_CHANNELS, &audioFormat->channels))
		goto fail;
	if(ioctl(od->fd, SNDCTL_DSP_SPEED, &audioFormat->sampleRate))
		goto fail;
	if(ioctl(od->fd, SNDCTL_DSP_SAMPLESIZE, &audioFormat->bits))
		goto fail;
	/*i = 1; if (ioctl(od->fd,SNDCTL_DSP_STEREO,&i)) err != 32; */

	audioOutput->open = 1;

	return 0;

fail:
	if(od->fd >= 0) close(od->fd);
	audioOutput->open = 0;
	ERROR("Error opening OSS device \"%s\": %s\n", od->device, 
			strerror(errno));
	return -1;
}

static void oss_closeDevice(AudioOutput * audioOutput) {
	OssData * od = audioOutput->data;

	if(od->fd >= 0) {
		close(od->fd);
		od->fd = -1;
	}

	audioOutput->open = 0;
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
	NULL /* sendMetadataFunc */
};

#endif /* HAVE_OSS */


