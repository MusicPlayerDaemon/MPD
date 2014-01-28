/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "AoOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <ao/ao.h>
#include <glib.h>

#include <string.h>

/* An ao_sample_format, with all fields set to zero: */
static ao_sample_format OUR_AO_FORMAT_INITIALIZER;

static unsigned ao_output_ref;

struct AoOutput {
	AudioOutput base;

	size_t write_size;
	int driver;
	ao_option *options;
	ao_device *device;

	AoOutput()
		:base(ao_output_plugin) {}

	bool Initialize(const config_param &param, Error &error) {
		return base.Configure(param, error);
	}

	bool Configure(const config_param &param, Error &error);
};

static constexpr Domain ao_output_domain("ao_output");

static void
ao_output_error(Error &error_r)
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
		error_r.SetErrno();
		return;
	}

	error_r.Set(ao_output_domain, errno, error);
}

inline bool
AoOutput::Configure(const config_param &param, Error &error)
{
	const char *value;

	options = nullptr;

	write_size = param.GetBlockValue("write_size", 1024u);

	if (ao_output_ref == 0) {
		ao_initialize();
	}
	ao_output_ref++;

	value = param.GetBlockValue("driver", "default");
	if (0 == strcmp(value, "default"))
		driver = ao_default_driver_id();
	else
		driver = ao_driver_id(value);

	if (driver < 0) {
		error.Format(ao_output_domain,
			     "\"%s\" is not a valid ao driver",
			     value);
		return false;
	}

	ao_info *ai = ao_driver_info(driver);
	if (ai == nullptr) {
		error.Set(ao_output_domain, "problems getting driver info");
		return false;
	}

	FormatDebug(ao_output_domain, "using ao driver \"%s\" for \"%s\"\n",
		    ai->short_name, param.GetBlockValue("name", nullptr));

	value = param.GetBlockValue("options", nullptr);
	if (value != nullptr) {
		gchar **_options = g_strsplit(value, ";", 0);

		for (unsigned i = 0; _options[i] != nullptr; ++i) {
			gchar **key_value = g_strsplit(_options[i], "=", 2);

			if (key_value[0] == nullptr || key_value[1] == nullptr) {
				error.Format(ao_output_domain,
					     "problems parsing options \"%s\"",
					     _options[i]);
				return false;
			}

			ao_append_option(&options, key_value[0],
					 key_value[1]);

			g_strfreev(key_value);
		}

		g_strfreev(_options);
	}

	return true;
}

static AudioOutput *
ao_output_init(const config_param &param, Error &error)
{
	AoOutput *ad = new AoOutput();

	if (!ad->Initialize(param, error)) {
		delete ad;
		return nullptr;
	}

	if (!ad->Configure(param, error)) {
		delete ad;
		return nullptr;
	}

	return &ad->base;
}

static void
ao_output_finish(AudioOutput *ao)
{
	AoOutput *ad = (AoOutput *)ao;

	ao_free_options(ad->options);
	delete ad;

	ao_output_ref--;

	if (ao_output_ref == 0)
		ao_shutdown();
}

static void
ao_output_close(AudioOutput *ao)
{
	AoOutput *ad = (AoOutput *)ao;

	ao_close(ad->device);
}

static bool
ao_output_open(AudioOutput *ao, AudioFormat &audio_format,
	       Error &error)
{
	ao_sample_format format = OUR_AO_FORMAT_INITIALIZER;
	AoOutput *ad = (AoOutput *)ao;

	switch (audio_format.format) {
	case SampleFormat::S8:
		format.bits = 8;
		break;

	case SampleFormat::S16:
		format.bits = 16;
		break;

	default:
		/* support for 24 bit samples in libao is currently
		   dubious, and until we have sorted that out,
		   convert everything to 16 bit */
		audio_format.format = SampleFormat::S16;
		format.bits = 16;
		break;
	}

	format.rate = audio_format.sample_rate;
	format.byte_format = AO_FMT_NATIVE;
	format.channels = audio_format.channels;

	ad->device = ao_open_live(ad->driver, &format, ad->options);

	if (ad->device == nullptr) {
		ao_output_error(error);
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
		char *out;
	} u;

	u.in = output_samples;
	return ao_play(device, u.out, num_bytes);
}

static size_t
ao_output_play(AudioOutput *ao, const void *chunk, size_t size,
	       Error &error)
{
	AoOutput *ad = (AoOutput *)ao;

	if (size > ad->write_size)
		size = ad->write_size;

	if (ao_play_deconst(ad->device, chunk, size) == 0) {
		ao_output_error(error);
		return 0;
	}

	return size;
}

const struct AudioOutputPlugin ao_output_plugin = {
	"ao",
	nullptr,
	ao_output_init,
	ao_output_finish,
	nullptr,
	nullptr,
	ao_output_open,
	ao_output_close,
	nullptr,
	nullptr,
	ao_output_play,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};
