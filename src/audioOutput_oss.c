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


#include "audio.h"

#ifdef HAVE_OSS

#include "conf.h"
#include "log.h"
#include "sig_handlers.h"

#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>

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

static typedef struct _OssData {
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

static void oss_initDriver(AudioOutput * audioOutput, ConfigParam * param) {
	char * test;
	BlockParam * bp = getBlockParam(param, "device");
	OssData * od = newOssData():
	
	audioOutput->data = od;

	if(!bp) {
		int fd;

		if(0 <= (fd = fopen("/dev/sound/dsp", O_WRONLY | O_NONBLOCK))) {
			od->device = strdup("/dev/sound/dsp");
		}
		else if(0 <= (fd = fopen("/dev/dsp", O_WRONLY | O_NONBLOCK))) {
			od->device = strdup("/dev/dsp");
		}
		else {
			ERROR("Error trying to open default OSS device "
				"specified at line %i\n", param->line);
			ERROR("Specify a OSS device and/or check your "
				"permissions\n");
			exit(EXIT_FAILURE);
		}

		close(od->fd);
		od->fd = -1;

		return;
	}

	od->device = strdup(bp->value);

	return;
}

static void oss_finishDriver(AudioOutput * audioOutput) {
	OssData * od = audioOutput->data;

	freeOssData(od);
}

static int oss_openDevice(AudioOutput * audioOutput,
		AudioFormat * audioFormat) 
{
	int i = AFMT_S16_LE, err = 0;
	if (audio_device && !isCurrentAudioFormat(audioFormat)) 
		closeAudioDevice();
	if (audio_device!=0)
		return 0;
	
	if (audioFormat)
		copyAudioFormat(&audio_format,audioFormat);

	blockSignals();
	audio_device = open("/dev/dsp", O_WRONLY);
	
	if (audio_device < 0) err |= 1;
	
	if (ioctl(audio_device,SNDCTL_DSP_SETFMT,&i))
		err |= 2;
	if (ioctl(audio_device,SNDCTL_DSP_CHANNELS, &audio_format.channels))
		err |= 4;
	if (ioctl(audio_device,SNDCTL_DSP_SPEED,&audio_format.sampleRate))
		err |= 8;
	if (ioctl(audio_device,SNDCTL_DSP_SAMPLESIZE,&audio_format.bits))
		err |= 16;
	/*i = 1; if (ioctl(audio_device,SNDCTL_DSP_STEREO,&i)) err != 32; */
	
	unblockSignals();
	
	if (err)
		ERROR("Error opening /dev/dsp: 0x%x\n");
	if (!audio_device)
		return -1;

	return 0;
}

static int oss_playAudio(AudioOutput * audioOutput, char * playChunk, 
		int size) 
{
	int send;
	int ret;

	if(audio_device==0) {
		ERROR("trying to play w/o the audio device being open!\n");
		return -1;
	}
	send = audio_write_size>size?size:audio_write_size;
	while (size > 0) {
		ret = write(audio_device,playChunk,send);
		if(ret<0) {
			audioError();
			ERROR("closing audio device due to write error\n");
			closeAudioDevice();
			return -1;
		}
		playChunk+=ret;
		size-=ret;
	}

	return 0;
}

static void oss_closeDevice(AudioOutput * audioOutput) {
	if(audio_device) {
		blockSignals();
		close(audio_device);
		audio_device = 0;
		unblockSignals();
	}
}

AudioOutput ossPlugin =
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

AudioOutput ossPlugin =
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


