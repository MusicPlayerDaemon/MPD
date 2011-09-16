/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

/*
 * Warning: this plugin was not tested successfully.  I just couldn't
 * keep libffado2 from crashing.  Use at your own risk.
 *
 * For details, see my Debian bug reports:
 *
 *   http://bugs.debian.org/601657
 *   http://bugs.debian.org/601659
 *   http://bugs.debian.org/601663
 *
 */

#include "config.h"
#include "ffado_output_plugin.h"
#include "output_api.h"
#include "timer.h"

#include <glib.h>
#include <assert.h>

#include <libffado/ffado.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ffado"

enum {
	MAX_STREAMS = 8,
};

struct mpd_ffado_stream {
	/** libffado's stream number */
	int number;

	float *buffer;
};

struct mpd_ffado_device {
	struct audio_output base;

	char *device_name;
	int verbose;
	unsigned period_size, nb_buffers;

	ffado_device_t *dev;

	/**
	 * The current sample position inside the stream buffers.  New
	 * samples get appended at this position on all streams at the
	 * same time.  When the buffers are full
	 * (buffer_position==period_size),
	 * ffado_streaming_transfer_playback_buffers() gets called to
	 * hand them over to libffado.
	 */
	unsigned buffer_position;

	/**
	 * The number of streams which are really used by MPD.
	 */
	int num_streams;
	struct mpd_ffado_stream streams[MAX_STREAMS];
};

static inline GQuark
ffado_output_quark(void)
{
	return g_quark_from_static_string("ffado_output");
}

static struct audio_output *
ffado_init(const struct config_param *param,
	   GError **error_r)
{
	g_debug("using libffado version %s, API=%d",
		ffado_get_version(), ffado_get_api_version());

	struct mpd_ffado_device *fd = g_new(struct mpd_ffado_device, 1);
	if (!ao_base_init(&fd->base, &ffado_output_plugin, param, error_r)) {
		g_free(fd);
		return NULL;
	}

	fd->device_name = config_dup_block_string(param, "device", NULL);
	fd->verbose = config_get_block_unsigned(param, "verbose", 0);

	fd->period_size = config_get_block_unsigned(param, "period_size",
						    1024);
	if (fd->period_size == 0 || fd->period_size > 1024 * 1024) {
		ao_base_finish(&fd->base);
		g_set_error(error_r, ffado_output_quark(), 0,
			    "invalid period_size setting");
		return false;
	}

	fd->nb_buffers = config_get_block_unsigned(param, "nb_buffers", 3);
	if (fd->nb_buffers == 0 || fd->nb_buffers > 1024) {
		ao_base_finish(&fd->base);
		g_set_error(error_r, ffado_output_quark(), 0,
			    "invalid nb_buffers setting");
		return false;
	}

	return &fd->base;
}

static void
ffado_finish(struct audio_output *ao)
{
	struct mpd_ffado_device *fd = (struct mpd_ffado_device *)ao;

	g_free(fd->device_name);
	ao_base_finish(&fd->base);
	g_free(fd);
}

static bool
ffado_configure_stream(ffado_device_t *dev, struct mpd_ffado_stream *stream,
		       GError **error_r)
{
	char *buffer = (char *)stream->buffer;
	if (ffado_streaming_set_playback_stream_buffer(dev, stream->number,
						       buffer) != 0) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "failed to configure stream buffer");
		return false;
	}

	if (ffado_streaming_playback_stream_onoff(dev, stream->number,
						  1) != 0) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "failed to disable stream");
		return false;
	}

	return true;
}

static bool
ffado_configure(struct mpd_ffado_device *fd, struct audio_format *audio_format,
		GError **error_r)
{
	assert(fd != NULL);
	assert(fd->dev != NULL);
	assert(audio_format->channels <= MAX_STREAMS);

	if (ffado_streaming_set_audio_datatype(fd->dev,
					       ffado_audio_datatype_float) != 0) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "ffado_streaming_set_audio_datatype() failed");
		return false;
	}

	int num_streams = ffado_streaming_get_nb_playback_streams(fd->dev);
	if (num_streams < 0) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "ffado_streaming_get_nb_playback_streams() failed");
		return false;
	}

	g_debug("there are %d playback streams", num_streams);

	fd->num_streams = 0;
	for (int i = 0; i < num_streams; ++i) {
		char name[256];
		ffado_streaming_get_playback_stream_name(fd->dev, i, name,
							 sizeof(name) - 1);

		ffado_streaming_stream_type type =
			ffado_streaming_get_playback_stream_type(fd->dev, i);
		if (type != ffado_stream_type_audio) {
			g_debug("stream %d name='%s': not an audio stream",
				i, name);
			continue;
		}

		if (fd->num_streams >= audio_format->channels) {
			g_debug("stream %d name='%s': ignoring",
				i, name);
			continue;
		}

		g_debug("stream %d name='%s'", i, name);

		struct mpd_ffado_stream *stream =
			&fd->streams[fd->num_streams++];

		stream->number = i;

		/* allocated buffer is zeroed = silence */
		stream->buffer = g_new0(float, fd->period_size);

		if (!ffado_configure_stream(fd->dev, stream, error_r))
			return false;
	}

	if (!audio_valid_channel_count(fd->num_streams)) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "invalid channel count from libffado: %u",
			    audio_format->channels);
		return false;
	}

	g_debug("configured %d audio streams", fd->num_streams);

	if (ffado_streaming_prepare(fd->dev) != 0) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "ffado_streaming_prepare() failed");
		return false;
	}

	if (ffado_streaming_start(fd->dev) != 0) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "ffado_streaming_start() failed");
		return false;
	}

	audio_format->channels = fd->num_streams;
	return true;
}

static bool
ffado_open(struct audio_output *ao, struct audio_format *audio_format,
	   GError **error_r)
{
	struct mpd_ffado_device *fd = (struct mpd_ffado_device *)ao;

	/* will be converted to floating point, choose best input
	   format */
	audio_format->format = SAMPLE_FORMAT_S24_P32;

	ffado_device_info_t device_info;
	memset(&device_info, 0, sizeof(device_info));
	if (fd->device_name != NULL) {
		device_info.nb_device_spec_strings = 1;
		device_info.device_spec_strings = &fd->device_name;
	}

	ffado_options_t options;
	memset(&options, 0, sizeof(options));
	options.sample_rate = audio_format->sample_rate;
	options.period_size = fd->period_size;
	options.nb_buffers = fd->nb_buffers;
	options.verbose = fd->verbose;

	fd->dev = ffado_streaming_init(device_info, options);
	if (fd->dev == NULL) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "ffado_streaming_init() failed");
		return false;
	}

	if (!ffado_configure(fd, audio_format, error_r)) {
		ffado_streaming_finish(fd->dev);

		for (int i = 0; i < fd->num_streams; ++i) {
			struct mpd_ffado_stream *stream = &fd->streams[i];
			g_free(stream->buffer);
		}

		return false;
	}

	fd->buffer_position = 0;

	return true;
}

static void
ffado_close(struct audio_output *ao)
{
	struct mpd_ffado_device *fd = (struct mpd_ffado_device *)ao;

	ffado_streaming_stop(fd->dev);
	ffado_streaming_finish(fd->dev);

	for (int i = 0; i < fd->num_streams; ++i) {
		struct mpd_ffado_stream *stream = &fd->streams[i];
		g_free(stream->buffer);
	}
}

static size_t
ffado_play(struct audio_output *ao, const void *chunk, size_t size,
	   GError **error_r)
{
	struct mpd_ffado_device *fd = (struct mpd_ffado_device *)ao;

	/* wait for prefious buffer to finish (if it was full) */

	if (fd->buffer_position >= fd->period_size) {
		switch (ffado_streaming_wait(fd->dev)) {
		case ffado_wait_ok:
		case ffado_wait_xrun:
			break;

		default:
			g_set_error(error_r, ffado_output_quark(), 0,
				    "ffado_streaming_wait() failed");
			return 0;
		}

		fd->buffer_position = 0;
	}

	/* copy samples to stream buffers, non-interleaved */

	const int32_t *p = chunk;
	unsigned num_frames = size / sizeof(*p) / fd->num_streams;
	if (num_frames > fd->period_size - fd->buffer_position)
		num_frames = fd->period_size - fd->buffer_position;

	for (unsigned i = num_frames; i > 0; --i) {
		for (int stream = 0; stream < fd->num_streams; ++stream)
			fd->streams[stream].buffer[fd->buffer_position] =
				*p++ / (float)(1 << 23);
		++fd->buffer_position;
	}

	/* if buffer full, transfer to device */

	if (fd->buffer_position >= fd->period_size &&
	    /* libffado documentation says this function returns -1 on
	       error, but that is a lie - it returns a boolean value,
	       and "false" means error */
	    !ffado_streaming_transfer_playback_buffers(fd->dev)) {
		g_set_error(error_r, ffado_output_quark(), 0,
			    "ffado_streaming_transfer_playback_buffers() failed");
		return 0;
	}

	return num_frames * sizeof(*p) * fd->num_streams;
}

const struct audio_output_plugin ffado_output_plugin = {
	.name = "ffado",
	.init = ffado_init,
	.finish = ffado_finish,
	.open = ffado_open,
	.close = ffado_close,
	.play = ffado_play,
};
