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

#include <ao/ao.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ao"

static int driverInitCount;

typedef struct _AoData {
	size_t writeSize;
	int driverId;
	ao_option *options;
	ao_device *device;
} AoData;

static AoData *newAoData(void)
{
	AoData *ret = g_malloc(sizeof(AoData));
	ret->device = NULL;
	ret->options = NULL;

	return ret;
}

static void audioOutputAo_error(const char *msg)
{
	const char *error;

	switch (errno) {
	case AO_ENODRIVER:
		error = "No such libao driver";
		break;

	case AO_ENOTLIVE:
		error = "This driver is not a libao live device";
		break;

	case AO_EBADOPTION:
		error = "Invalid libao option";
		break;

	case AO_EOPENDEVICE:
		error = "Cannot open the libao device";
		break;

	case AO_EFAIL:
		error = "Generic libao failure";
		break;

	default:
		error = strerror(errno);
	}

	g_warning("%s: %s\n", msg, error);
}

static void *
audioOutputAo_initDriver(struct audio_output *ao,
			 G_GNUC_UNUSED const struct audio_format *audio_format,
			 const struct config_param *param)
{
	ao_info *ai;
	AoData *ad = newAoData();
	const char *value;

	ad->writeSize = config_get_block_unsigned(param, "write_size", 1024);

	if (driverInitCount == 0) {
		ao_initialize();
	}
	driverInitCount++;

	value = config_get_block_string(param, "driver", "default");
	if (0 == strcmp(value, "default")) {
		ad->driverId = ao_default_driver_id();
	} else if ((ad->driverId = ao_driver_id(value)) < 0)
		g_error("\"%s\" is not a valid ao driver at line %i\n",
			value, param->line);

	if ((ai = ao_driver_info(ad->driverId)) == NULL) {
		g_error("problems getting driver info for device defined at line %i\n"
			"you may not have permission to the audio device\n", param->line);
	}

	g_debug("using ao driver \"%s\" for \"%s\"\n", ai->short_name,
		audio_output_get_name(ao));

	value = config_get_block_string(param, "options", NULL);
	if (value != NULL) {
		gchar **options = g_strsplit(value, ";", 0);

		for (unsigned i = 0; options[i] != NULL; ++i) {
			gchar **key_value = g_strsplit(options[i], "=", 2);

			if (key_value[0] == NULL || key_value[1] == NULL)
				g_error("problems parsing options \"%s\"\n",
					options[i]);

			ao_append_option(&ad->options, key_value[0],
					 key_value[1]);

			g_strfreev(key_value);
		}

		g_strfreev(options);
	}

	return ad;
}

static void freeAoData(AoData * ad)
{
	ao_free_options(ad->options);
	g_free(ad);
}

static void audioOutputAo_finishDriver(void *data)
{
	AoData *ad = (AoData *)data;
	freeAoData(ad);

	driverInitCount--;

	if (driverInitCount == 0)
		ao_shutdown();
}

static void audioOutputAo_dropBufferedAudio(G_GNUC_UNUSED void *data)
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

static bool
audioOutputAo_openDevice(void *data, struct audio_format *audio_format)
{
	ao_sample_format format;
	AoData *ad = (AoData *)data;

	if (ad->device) {
		audioOutputAo_closeDevice(ad);
	}

	/* support for 24 bit samples in libao is currently dubious,
	   and until we have sorted that out, resample everything to
	   16 bit */
	if (audio_format->bits > 16)
		audio_format->bits = 16;

	format.bits = audio_format->bits;
	format.rate = audio_format->sample_rate;
	format.byte_format = AO_FMT_NATIVE;
	format.channels = audio_format->channels;

	ad->device = ao_open_live(ad->driverId, &format, ad->options);

	if (ad->device == NULL) {
		audioOutputAo_error("Failed to open libao");
		return false;
	}

	return true;
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

static size_t
audioOutputAo_play(void *data, const char *playChunk, size_t size)
{
	AoData *ad = (AoData *)data;

	if (ad->device == NULL)
		return false;

	if (size > ad->writeSize)
		size = ad->writeSize;

	if (ao_play_deconst(ad->device, playChunk, size) == 0) {
		audioOutputAo_error("Closing libao device due to play error");
		return 0;
	}

	return size;
}

const struct audio_output_plugin aoPlugin = {
	.name = "ao",
	.init = audioOutputAo_initDriver,
	.finish = audioOutputAo_finishDriver,
	.open = audioOutputAo_openDevice,
	.play = audioOutputAo_play,
	.cancel = audioOutputAo_dropBufferedAudio,
	.close = audioOutputAo_closeDevice,
};
