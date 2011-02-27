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

#include "../output_api.h"
#include "config.h"

#include <assert.h>

#include <glib.h>
#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "jack"

static const size_t jack_sample_size = sizeof(jack_default_audio_sample_t);

static const char *const port_names[2] = {
	"left", "right",
};

struct jack_data {
	const char *name;

	/* configuration */
	char *output_ports[2];
	int ringbuffer_size;

	/* the current audio format */
	struct audio_format audio_format;

	/* jack library stuff */
	jack_port_t *ports[2];
	jack_client_t *client;
	jack_ringbuffer_t *ringbuffer[2];

	bool shutdown;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
jack_output_quark(void)
{
	return g_quark_from_static_string("jack_output");
}

static void
mpd_jack_client_free(struct jack_data *jd)
{
	assert(jd != NULL);

	if (jd->client != NULL) {
		jack_deactivate(jd->client);
		jack_client_close(jd->client);
		jd->client = NULL;
	}

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ringbuffer); ++i) {
		if (jd->ringbuffer[i] != NULL) {
			jack_ringbuffer_free(jd->ringbuffer[i]);
			jd->ringbuffer[i] = NULL;
		}
	}
}

static void
mpd_jack_free(struct jack_data *jd)
{
	assert(jd != NULL);

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->output_ports); ++i)
		g_free(jd->output_ports[i]);

	g_free(jd);
}

static void
mpd_jack_finish(void *data)
{
	struct jack_data *jd = data;
	mpd_jack_free(jd);
}

static int
mpd_jack_process(jack_nframes_t nframes, void *arg)
{
	struct jack_data *jd = (struct jack_data *) arg;
	jack_default_audio_sample_t *out;
	size_t available;

	if (nframes <= 0)
		return 0;

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ringbuffer); ++i) {
		available = jack_ringbuffer_read_space(jd->ringbuffer[i]);
		assert(available % jack_sample_size == 0);
		available /= jack_sample_size;
		if (available > nframes)
			available = nframes;

		out = jack_port_get_buffer(jd->ports[i], nframes);
		jack_ringbuffer_read(jd->ringbuffer[i],
				     (char *)out,
				     available * jack_sample_size);

		while (available < nframes)
			/* ringbuffer underrun, fill with silence */
			out[available++] = 0.0;
	}

	return 0;
}

static void
mpd_jack_shutdown(void *arg)
{
	struct jack_data *jd = (struct jack_data *) arg;
	jd->shutdown = true;
}

static void
set_audioformat(struct jack_data *jd, struct audio_format *audio_format)
{
	audio_format->sample_rate = jack_get_sample_rate(jd->client);
	audio_format->channels = 2;

	if (audio_format->bits != 16 && audio_format->bits != 24)
		audio_format->bits = 24;
}

static void
mpd_jack_error(const char *msg)
{
	g_warning("%s", msg);
}

#ifdef HAVE_JACK_SET_INFO_FUNCTION
static void
mpd_jack_info(const char *msg)
{
	g_message("%s", msg);
}
#endif

static void *
mpd_jack_init(G_GNUC_UNUSED const struct audio_format *audio_format,
	      const struct config_param *param, GError **error)
{
	struct jack_data *jd;
	const char *value;

	jd = g_new(struct jack_data, 1);
	jd->name = config_get_block_string(param, "name", "mpd_jack");

	g_debug("mpd_jack_init (pid=%d)", getpid());

	value = config_get_block_string(param, "ports", NULL);
	if (value != NULL) {
		char **ports = g_strsplit(value, ",", 0);

		if (ports[0] == NULL || ports[1] == NULL || ports[2] != NULL) {
			g_set_error(error, jack_output_quark(), 0,
				    "two port names expected in line %d",
				    param->line);
			return NULL;
		}

		jd->output_ports[0] = ports[0];
		jd->output_ports[1] = ports[1];

		g_free(ports);
	} else {
		jd->output_ports[0] = NULL;
		jd->output_ports[1] = NULL;
	}

	jd->ringbuffer_size =
		config_get_block_unsigned(param, "ringbuffer_size", 32768);

	jack_set_error_function(mpd_jack_error);

#ifdef HAVE_JACK_SET_INFO_FUNCTION
	jack_set_info_function(mpd_jack_info);
#endif

	return jd;
}

static bool
mpd_jack_test_default_device(void)
{
	return true;
}

static bool
mpd_jack_connect(struct jack_data *jd, GError **error)
{
	const char *output_ports[2], **jports;

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ringbuffer); ++i)
		jd->ringbuffer[i] =
			jack_ringbuffer_create(jd->ringbuffer_size);

	jd->shutdown = false;

	if ((jd->client = jack_client_new(jd->name)) == NULL) {
		g_set_error(error, jack_output_quark(), 0,
			    "Failed to connect to JACK server");
		return false;
	}

	jack_set_process_callback(jd->client, mpd_jack_process, jd);
	jack_on_shutdown(jd->client, mpd_jack_shutdown, jd);

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ports); ++i) {
		jd->ports[i] = jack_port_register(jd->client, port_names[i],
						  JACK_DEFAULT_AUDIO_TYPE,
						  JackPortIsOutput, 0);
		if (jd->ports[i] == NULL) {
			g_set_error(error, jack_output_quark(), 0,
				    "Cannot register output port \"%s\"",
				    port_names[i]);
			return false;
		}
	}

	if ( jack_activate(jd->client) ) {
		g_set_error(error, jack_output_quark(), 0,
			    "cannot activate client");
		return false;
	}

	if (jd->output_ports[1] == NULL) {
		/* no output ports were configured - ask libjack for
		   defaults */
		jports = jack_get_ports(jd->client, NULL, NULL,
					JackPortIsPhysical | JackPortIsInput);
		if (jports == NULL) {
			g_set_error(error, jack_output_quark(), 0,
				    "no ports found");
			return false;
		}

		output_ports[0] = jports[0];
		output_ports[1] = jports[1] != NULL ? jports[1] : jports[0];

		g_debug("output_ports: %s %s", jports[0], jports[1]);
	} else {
		/* use the configured output ports */

		output_ports[0] = jd->output_ports[0];
		output_ports[1] = jd->output_ports[1];

		jports = NULL;
	}

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ports); ++i) {
		int ret;

		ret = jack_connect(jd->client, jack_port_name(jd->ports[i]),
				   output_ports[i]);
		if (ret != 0) {
			g_set_error(error, jack_output_quark(), 0,
				    "Not a valid JACK port: %s",
				    output_ports[i]);

			if (jports != NULL)
				free(jports);

			return false;
		}
	}

	if (jports != NULL)
		free(jports);

	return true;
}

static bool
mpd_jack_open(void *data, struct audio_format *audio_format, GError **error)
{
	struct jack_data *jd = data;

	assert(jd != NULL);

	if (!mpd_jack_connect(jd, error)) {
		mpd_jack_client_free(jd);
		return false;
	}

	set_audioformat(jd, audio_format);
	jd->audio_format = *audio_format;

	return true;
}

static void
mpd_jack_close(G_GNUC_UNUSED void *data)
{
	struct jack_data *jd = data;

	mpd_jack_client_free(jd);
}

static void
mpd_jack_cancel (G_GNUC_UNUSED void *data)
{
}

static inline jack_default_audio_sample_t
sample_16_to_jack(int16_t sample)
{
	return sample / (jack_default_audio_sample_t)(1 << (16 - 1));
}

static void
mpd_jack_write_samples_16(struct jack_data *jd, const int16_t *src,
			  unsigned num_samples)
{
	jack_default_audio_sample_t sample;

	while (num_samples-- > 0) {
		sample = sample_16_to_jack(*src++);
		jack_ringbuffer_write(jd->ringbuffer[0], (void*)&sample,
				      sizeof(sample));

		sample = sample_16_to_jack(*src++);
		jack_ringbuffer_write(jd->ringbuffer[1], (void*)&sample,
				      sizeof(sample));
	}
}

static inline jack_default_audio_sample_t
sample_24_to_jack(int32_t sample)
{
	return sample / (jack_default_audio_sample_t)(1 << (24 - 1));
}

static void
mpd_jack_write_samples_24(struct jack_data *jd, const int32_t *src,
			  unsigned num_samples)
{
	jack_default_audio_sample_t sample;

	while (num_samples-- > 0) {
		sample = sample_24_to_jack(*src++);
		jack_ringbuffer_write(jd->ringbuffer[0], (void*)&sample,
				      sizeof(sample));

		sample = sample_24_to_jack(*src++);
		jack_ringbuffer_write(jd->ringbuffer[1], (void*)&sample,
				      sizeof(sample));
	}
}

static void
mpd_jack_write_samples(struct jack_data *jd, const void *src,
		       unsigned num_samples)
{
	switch (jd->audio_format.bits) {
	case 16:
		mpd_jack_write_samples_16(jd, (const int16_t*)src,
					  num_samples);
		break;

	case 24:
		mpd_jack_write_samples_24(jd, (const int32_t*)src,
					  num_samples);
		break;

	default:
		assert(false);
	}
}

static size_t
mpd_jack_play(void *data, const void *chunk, size_t size, GError **error)
{
	struct jack_data *jd = data;
	const size_t frame_size = audio_format_frame_size(&jd->audio_format);
	size_t space = 0, space1;

	assert(size % frame_size == 0);
	size /= frame_size;

	while (true) {
		if (jd->shutdown) {
			g_set_error(error, jack_output_quark(), 0,
				    "Refusing to play, because "
				    "there is no client thread");
			return 0;
		}

		space = jack_ringbuffer_write_space(jd->ringbuffer[0]);
		space1 = jack_ringbuffer_write_space(jd->ringbuffer[1]);
		if (space > space1)
			/* send data symmetrically */
			space = space1;

		if (space >= jack_sample_size)
			break;

		/* XXX do something more intelligent to
		   synchronize */
		g_usleep(1000);
	}

	space /= jack_sample_size;
	if (space < size)
		size = space;

	mpd_jack_write_samples(jd, chunk, size);
	return size * frame_size;
}

const struct audio_output_plugin jackPlugin = {
	.name = "jack",
	.test_default_device = mpd_jack_test_default_device,
	.init = mpd_jack_init,
	.finish = mpd_jack_finish,
	.open = mpd_jack_open,
	.play = mpd_jack_play,
	.cancel = mpd_jack_cancel,
	.close = mpd_jack_close,
};
