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

#include "config.h"
#include "output_api.h"
#include "timer.h"

#include <glib.h>

#ifndef HAVE_OSX
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "openal"

/* should be enough for buffer size = 2048 */
#define NUM_BUFFERS 16

struct openal_data {
	const char *device_name;
	ALCdevice *device;
	ALCcontext *context;
	Timer *timer;
	ALuint buffers[NUM_BUFFERS];
	int filled;
	ALuint source;
	ALenum format;
	ALuint frequency;
};

static inline GQuark
openal_output_quark(void)
{
	return g_quark_from_static_string("openal_output");
}

static ALenum
openal_audio_format(struct audio_format *audio_format)
{
	/* Only 8 and 16 bit samples are supported */
	if (audio_format->bits != 16 && audio_format->bits != 8)
		audio_format->bits = 16;

	switch (audio_format->bits)
	{
	case 16:
		if (audio_format->channels == 2)
			return AL_FORMAT_STEREO16;
		if (audio_format->channels == 1)
			return AL_FORMAT_MONO16;
		break;

	case 8:
		if (audio_format->channels == 2)
			return AL_FORMAT_STEREO8;
		if (audio_format->channels == 1)
			return AL_FORMAT_MONO8;
		break;
	}

	return 0;
}

static bool
openal_setup_context(struct openal_data *od,
		     GError **error)
{
	od->device = alcOpenDevice(od->device_name);

	if (od->device == NULL) {
		g_set_error(error, openal_output_quark(), 0,
			    "Error opening OpenAL device \"%s\"\n",
			    od->device_name);
		return false;
	}

	od->context = alcCreateContext(od->device, NULL);

	if (od->context == NULL) {
		g_set_error(error, openal_output_quark(), 0,
			    "Error creating context for \"%s\"\n",
			    od->device_name);
		alcCloseDevice(od->device);
		return false;
	}

	return true;
}

static void
openal_unqueue_buffers(struct openal_data *od)
{
	ALint num;
	ALuint buffer;

	alGetSourcei(od->source, AL_BUFFERS_QUEUED, &num);

	while (num--) {
		alSourceUnqueueBuffers(od->source, 1, &buffer);
	}
}

static void *
openal_init(G_GNUC_UNUSED const struct audio_format *audio_format,
	    const struct config_param *param,
	    G_GNUC_UNUSED GError **error)
{
	const char *device_name = config_get_block_string(param, "device", NULL);
	struct openal_data *od;

	if (device_name == NULL) {
		device_name = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
	}

	od = g_new(struct openal_data, 1);
	od->device_name = device_name;

	return od;
}

static void
openal_finish(void *data)
{
	struct openal_data *od = data;

	g_free(od);
}

static bool
openal_open(void *data, struct audio_format *audio_format,
	    GError **error)
{
	struct openal_data *od = data;

	od->format = openal_audio_format(audio_format);

	if (!od->format) {
		struct audio_format_string s;
		g_set_error(error, openal_output_quark(), 0,
			    "Unsupported audio format: %s",
			    audio_format_to_string(audio_format, &s));
		return false;
	}

	if (!openal_setup_context(od, error)) {
		return false;
	}

	alcMakeContextCurrent(od->context);
	alGenBuffers(NUM_BUFFERS, od->buffers);

	if (alGetError() != AL_NO_ERROR) {
		g_set_error(error, openal_output_quark(), 0,
			    "Failed to generate buffers");
		return false;
	}

	alGenSources(1, &od->source);

	if (alGetError() != AL_NO_ERROR) {
		g_set_error(error, openal_output_quark(), 0,
			    "Failed to generate source");
		alDeleteBuffers(NUM_BUFFERS, od->buffers);
		return false;
	}

	od->filled = 0;
	od->timer = timer_new(audio_format);
	od->frequency = audio_format->sample_rate;

	return true;
}

static void
openal_close(void *data)
{
	struct openal_data *od = data;

	timer_free(od->timer);
	alcMakeContextCurrent(od->context);
	alDeleteSources(1, &od->source);
	alDeleteBuffers(NUM_BUFFERS, od->buffers);
	alcDestroyContext(od->context);
	alcCloseDevice(od->device);
}

static size_t
openal_play(void *data, const void *chunk, size_t size,
	    G_GNUC_UNUSED GError **error)
{
	struct openal_data *od = data;
	ALuint buffer;
	ALint num, state;

	if (alcGetCurrentContext() != od->context) {
		alcMakeContextCurrent(od->context);
	}

	alGetSourcei(od->source, AL_BUFFERS_PROCESSED, &num);

	if (od->filled < NUM_BUFFERS) {
		/* fill all buffers */
		buffer = od->buffers[od->filled];
		od->filled++;
	} else {
		/* wait for processed buffer */
		while (num < 1) {
			if (!od->timer->started) {
				timer_start(od->timer);
			} else {
				timer_sync(od->timer);
			}

			timer_add(od->timer, size);

			alGetSourcei(od->source, AL_BUFFERS_PROCESSED, &num);
		}

		alSourceUnqueueBuffers(od->source, 1, &buffer);
	}

	alBufferData(buffer, od->format, chunk, size, od->frequency);
	alSourceQueueBuffers(od->source, 1, &buffer);
	alGetSourcei(od->source, AL_SOURCE_STATE, &state);

	if (state != AL_PLAYING) {
		alSourcePlay(od->source);
	}

	return size;
}

static void
openal_cancel(void *data)
{
	struct openal_data *od = data;

	od->filled = 0;
	alcMakeContextCurrent(od->context);
	alSourceStop(od->source);
	openal_unqueue_buffers(od);
}

const struct audio_output_plugin openal_output_plugin = {
	.name = "openal",
	.init = openal_init,
	.finish = openal_finish,
	.open = openal_open,
	.close = openal_close,
	.play = openal_play,
	.cancel = openal_cancel,
};
