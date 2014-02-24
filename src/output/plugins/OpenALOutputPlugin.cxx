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
#include "OpenALOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <unistd.h>

#ifndef __APPLE__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#endif

/* should be enough for buffer size = 2048 */
#define NUM_BUFFERS 16

struct OpenALOutput {
	AudioOutput base;

	const char *device_name;
	ALCdevice *device;
	ALCcontext *context;
	ALuint buffers[NUM_BUFFERS];
	unsigned filled;
	ALuint source;
	ALenum format;
	ALuint frequency;

	OpenALOutput()
		:base(openal_output_plugin) {}

	bool Initialize(const config_param &param, Error &error_r) {
		return base.Configure(param, error_r);
	}
};

static constexpr Domain openal_output_domain("openal_output");

static ALenum
openal_audio_format(AudioFormat &audio_format)
{
	/* note: cannot map SampleFormat::S8 to AL_FORMAT_STEREO8 or
	   AL_FORMAT_MONO8 since OpenAL expects unsigned 8 bit
	   samples, while MPD uses signed samples */

	switch (audio_format.format) {
	case SampleFormat::S16:
		if (audio_format.channels == 2)
			return AL_FORMAT_STEREO16;
		if (audio_format.channels == 1)
			return AL_FORMAT_MONO16;

		/* fall back to mono */
		audio_format.channels = 1;
		return openal_audio_format(audio_format);

	default:
		/* fall back to 16 bit */
		audio_format.format = SampleFormat::S16;
		return openal_audio_format(audio_format);
	}
}

gcc_pure
static inline ALint
openal_get_source_i(const OpenALOutput *od, ALenum param)
{
	ALint value;
	alGetSourcei(od->source, param, &value);
	return value;
}

gcc_pure
static inline bool
openal_has_processed(const OpenALOutput *od)
{
	return openal_get_source_i(od, AL_BUFFERS_PROCESSED) > 0;
}

gcc_pure
static inline ALint
openal_is_playing(const OpenALOutput *od)
{
	return openal_get_source_i(od, AL_SOURCE_STATE) == AL_PLAYING;
}

static bool
openal_setup_context(OpenALOutput *od, Error &error)
{
	od->device = alcOpenDevice(od->device_name);

	if (od->device == nullptr) {
		error.Format(openal_output_domain,
			     "Error opening OpenAL device \"%s\"",
			     od->device_name);
		return false;
	}

	od->context = alcCreateContext(od->device, nullptr);

	if (od->context == nullptr) {
		error.Format(openal_output_domain,
			     "Error creating context for \"%s\"",
			     od->device_name);
		alcCloseDevice(od->device);
		return false;
	}

	return true;
}

static AudioOutput *
openal_init(const config_param &param, Error &error)
{
	const char *device_name = param.GetBlockValue("device");
	if (device_name == nullptr) {
		device_name = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
	}

	OpenALOutput *od = new OpenALOutput();
	if (!od->Initialize(param, error)) {
		delete od;
		return nullptr;
	}

	od->device_name = device_name;

	return &od->base;
}

static void
openal_finish(AudioOutput *ao)
{
	OpenALOutput *od = (OpenALOutput *)ao;

	delete od;
}

static bool
openal_open(AudioOutput *ao, AudioFormat &audio_format,
	    Error &error)
{
	OpenALOutput *od = (OpenALOutput *)ao;

	od->format = openal_audio_format(audio_format);

	if (!openal_setup_context(od, error)) {
		return false;
	}

	alcMakeContextCurrent(od->context);
	alGenBuffers(NUM_BUFFERS, od->buffers);

	if (alGetError() != AL_NO_ERROR) {
		error.Set(openal_output_domain, "Failed to generate buffers");
		return false;
	}

	alGenSources(1, &od->source);

	if (alGetError() != AL_NO_ERROR) {
		error.Set(openal_output_domain, "Failed to generate source");
		alDeleteBuffers(NUM_BUFFERS, od->buffers);
		return false;
	}

	od->filled = 0;
	od->frequency = audio_format.sample_rate;

	return true;
}

static void
openal_close(AudioOutput *ao)
{
	OpenALOutput *od = (OpenALOutput *)ao;

	alcMakeContextCurrent(od->context);
	alDeleteSources(1, &od->source);
	alDeleteBuffers(NUM_BUFFERS, od->buffers);
	alcDestroyContext(od->context);
	alcCloseDevice(od->device);
}

static unsigned
openal_delay(AudioOutput *ao)
{
	OpenALOutput *od = (OpenALOutput *)ao;

	return od->filled < NUM_BUFFERS || openal_has_processed(od)
		? 0
		/* we don't know exactly how long we must wait for the
		   next buffer to finish, so this is a random
		   guess: */
		: 50;
}

static size_t
openal_play(AudioOutput *ao, const void *chunk, size_t size,
	    gcc_unused Error &error)
{
	OpenALOutput *od = (OpenALOutput *)ao;
	ALuint buffer;

	if (alcGetCurrentContext() != od->context) {
		alcMakeContextCurrent(od->context);
	}

	if (od->filled < NUM_BUFFERS) {
		/* fill all buffers */
		buffer = od->buffers[od->filled];
		od->filled++;
	} else {
		/* wait for processed buffer */
		while (!openal_has_processed(od))
			usleep(10);

		alSourceUnqueueBuffers(od->source, 1, &buffer);
	}

	alBufferData(buffer, od->format, chunk, size, od->frequency);
	alSourceQueueBuffers(od->source, 1, &buffer);

	if (!openal_is_playing(od))
		alSourcePlay(od->source);

	return size;
}

static void
openal_cancel(AudioOutput *ao)
{
	OpenALOutput *od = (OpenALOutput *)ao;

	od->filled = 0;
	alcMakeContextCurrent(od->context);
	alSourceStop(od->source);

	/* force-unqueue all buffers */
	alSourcei(od->source, AL_BUFFER, 0);
	od->filled = 0;
}

const struct AudioOutputPlugin openal_output_plugin = {
	"openal",
	nullptr,
	openal_init,
	openal_finish,
	nullptr,
	nullptr,
	openal_open,
	openal_close,
	openal_delay,
	nullptr,
	openal_play,
	nullptr,
	openal_cancel,
	nullptr,
	nullptr,
};
