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

#ifdef HAVE_AO

#include "../utils.h"
#include "../log.h"

#include <ao/ao.h>

static int driverInitCount;

typedef struct _AoData {
	int writeSize;
	int driverId;
	ao_option *options;
	ao_device *device;
} AoData;

static AoData *newAoData(void)
{
	AoData *ret = xmalloc(sizeof(AoData));
	ret->device = NULL;
	ret->options = NULL;

	return ret;
}

static void audioOutputAo_error(void)
{
	if (errno == AO_ENOTLIVE) {
		ERROR("not a live ao device\n");
	} else if (errno == AO_EOPENDEVICE) {
		ERROR("not able to open audio device\n");
	} else if (errno == AO_EBADOPTION) {
		ERROR("bad driver option\n");
	}
}

static void *audioOutputAo_initDriver(struct audio_output *ao,
				      mpd_unused const struct audio_format *audio_format,
				      ConfigParam * param)
{
	ao_info *ai;
	char *duplicated;
	char *stk1;
	char *stk2;
	char *n1;
	char *key;
	char *value;
	char *test;
	AoData *ad = newAoData();
	BlockParam *blockParam;

	if ((blockParam = getBlockParam(param, "write_size"))) {
		ad->writeSize = strtol(blockParam->value, &test, 10);
		if (*test != '\0') {
			FATAL("\"%s\" is not a valid write size at line %i\n",
			      blockParam->value, blockParam->line);
		}
	} else
		ad->writeSize = 1024;

	if (driverInitCount == 0) {
		ao_initialize();
	}
	driverInitCount++;

	blockParam = getBlockParam(param, "driver");

	if (!blockParam || 0 == strcmp(blockParam->value, "default")) {
		ad->driverId = ao_default_driver_id();
	} else if ((ad->driverId = ao_driver_id(blockParam->value)) < 0) {
		FATAL("\"%s\" is not a valid ao driver at line %i\n",
		      blockParam->value, blockParam->line);
	}

	if ((ai = ao_driver_info(ad->driverId)) == NULL) {
		FATAL("problems getting driver info for device defined at line %i\n"
		      "you may not have permission to the audio device\n", param->line);
	}

	DEBUG("using ao driver \"%s\" for \"%s\"\n", ai->short_name,
	      audio_output_get_name(ao));

	blockParam = getBlockParam(param, "options");

	if (blockParam) {
		duplicated = xstrdup(blockParam->value);
	} else
		duplicated = xstrdup("");

	if (strlen(duplicated)) {
		stk1 = NULL;
		n1 = strtok_r(duplicated, ";", &stk1);
		while (n1) {
			stk2 = NULL;
			key = strtok_r(n1, "=", &stk2);
			if (!key)
				FATAL("problems parsing options \"%s\"\n", n1);
			/*found = 0;
			   for(i=0;i<ai->option_count;i++) {
			   if(strcmp(ai->options[i],key)==0) {
			   found = 1;
			   break;
			   }
			   }
			   if(!found) {
			   FATAL("\"%s\" is not an option for "
			   "\"%s\" ao driver\n",key,
			   ai->short_name);
			   } */
			value = strtok_r(NULL, "", &stk2);
			if (!value)
				FATAL("problems parsing options \"%s\"\n", n1);
			ao_append_option(&ad->options, key, value);
			n1 = strtok_r(NULL, ";", &stk1);
		}
	}
	free(duplicated);

	return ad;
}

static void freeAoData(AoData * ad)
{
	ao_free_options(ad->options);
	free(ad);
}

static void audioOutputAo_finishDriver(void *data)
{
	AoData *ad = (AoData *)data;
	freeAoData(ad);

	driverInitCount--;

	if (driverInitCount == 0)
		ao_shutdown();
}

static void audioOutputAo_dropBufferedAudio(mpd_unused void *data)
{
	/* not supported by libao */
}

static void audioOutputAo_closeDevice(void *data)
{
	AoData *ad = (AoData *)data;

	if (ad->device) {
		ao_close(ad->device);
		ad->device = NULL;
	}
}

static int audioOutputAo_openDevice(void *data,
				    struct audio_format *audio_format)
{
	ao_sample_format format;
	AoData *ad = (AoData *)data;

	if (ad->device) {
		audioOutputAo_closeDevice(ad);
	}

	format.bits = audio_format->bits;
	format.rate = audio_format->sampleRate;
	format.byte_format = AO_FMT_NATIVE;
	format.channels = audio_format->channels;

	ad->device = ao_open_live(ad->driverId, &format, ad->options);

	if (ad->device == NULL)
		return -1;

	return 0;
}

/**
 * For whatever reason, libao wants a non-const pointer.  Let's hope
 * it does not write to the buffer, and use the union deconst hack to
 * work around this API misdesign.
 */
static int ao_play_deconst(ao_device *device, const void *output_samples,
			   uint_32 num_bytes)
{
	union {
		const void *in;
		void *out;
	} u;

	u.in = output_samples;
	return ao_play(device, u.out, num_bytes);
}

static int audioOutputAo_play(void *data, const char *playChunk, size_t size)
{
	AoData *ad = (AoData *)data;
	size_t chunk_size;

	if (ad->device == NULL)
		return -1;

	while (size > 0) {
		chunk_size = (size_t)ad->writeSize > size
			? size : (size_t)ad->writeSize;

		if (ao_play_deconst(ad->device, playChunk, chunk_size) == 0) {
			audioOutputAo_error();
			ERROR("closing audio device due to write error\n");
			audioOutputAo_closeDevice(ad);
			return -1;
		}

		playChunk += chunk_size;
		size -= chunk_size;
	}

	return 0;
}

const struct audio_output_plugin aoPlugin = {
	"ao",
	NULL,
	audioOutputAo_initDriver,
	audioOutputAo_finishDriver,
	audioOutputAo_openDevice,
	audioOutputAo_play,
	audioOutputAo_dropBufferedAudio,
	audioOutputAo_closeDevice,
	NULL,	/* sendMetadataFunc */
};

#else

DISABLED_AUDIO_OUTPUT_PLUGIN(aoPlugin)
#endif
