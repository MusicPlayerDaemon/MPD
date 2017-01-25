/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "system/Error.hxx"
#include "util/DivideString.hxx"
#include "util/SplitString.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <ao/ao.h>

#include <string.h>

/* An ao_sample_format, with all fields set to zero: */
static ao_sample_format OUR_AO_FORMAT_INITIALIZER;

static unsigned ao_output_ref;

struct AoOutput {
	AudioOutput base;

	const size_t write_size;
	int driver;
	ao_option *options = nullptr;
	ao_device *device;

	AoOutput(const ConfigBlock &block);
};

static constexpr Domain ao_output_domain("ao_output");


static std::system_error
MakeAoError()
{
	const char *error = "Unknown libao failure";

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
	}

	return MakeErrno(errno, error);
}

AoOutput::AoOutput(const ConfigBlock &block)
	:base(ao_output_plugin, block),
	 write_size(block.GetBlockValue("write_size", 1024u))
{
	if (ao_output_ref == 0) {
		ao_initialize();
	}
	ao_output_ref++;

	const char *value = block.GetBlockValue("driver", "default");
	if (0 == strcmp(value, "default"))
		driver = ao_default_driver_id();
	else
		driver = ao_driver_id(value);

	if (driver < 0)
		throw FormatRuntimeError("\"%s\" is not a valid ao driver",
					 value);

	ao_info *ai = ao_driver_info(driver);
	if (ai == nullptr)
		throw std::runtime_error("problems getting driver info");

	FormatDebug(ao_output_domain, "using ao driver \"%s\" for \"%s\"\n",
		    ai->short_name, block.GetBlockValue("name", nullptr));

	value = block.GetBlockValue("options", nullptr);
	if (value != nullptr) {
		for (const auto &i : SplitString(value, ';')) {
			const DivideString ss(i.c_str(), '=', true);

			if (!ss.IsDefined())
				throw FormatRuntimeError("problems parsing options \"%s\"",
					     i.c_str());

			ao_append_option(&options, ss.GetFirst(), ss.GetSecond());
		}
	}
}

static AudioOutput *
ao_output_init(const ConfigBlock &block)
{
	return &(new AoOutput(block))->base;
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

static void
ao_output_open(AudioOutput *ao, AudioFormat &audio_format)
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

	if (ad->device == nullptr)
		throw MakeAoError();
}

static size_t
ao_output_play(AudioOutput *ao, const void *chunk, size_t size)
{
	AoOutput *ad = (AoOutput *)ao;

	if (size > ad->write_size)
		size = ad->write_size;

	/* For whatever reason, libao wants a non-const pointer.
	   Let's hope it does not write to the buffer, and use the
	   union deconst hack to * work around this API misdesign. */
	char *data = const_cast<char *>((const char *)chunk);

	if (ao_play(ad->device, data, size) == 0)
		throw MakeAoError();

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
