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

enum {
	MAX_PORTS = 16,
};

static const size_t sample_size = sizeof(jack_default_audio_sample_t);

struct jack_data {
	/**
	 * libjack options passed to jack_client_open().
	 */
	jack_options_t options;

	const char *name;

	const char *server_name;

	/* configuration */

	char *source_ports[MAX_PORTS];
	unsigned num_source_ports;

	char *destination_ports[MAX_PORTS];
	unsigned num_destination_ports;

	size_t ringbuffer_size;

	/* the current audio format */
	struct audio_format audio_format;

	/* jack library stuff */
	jack_port_t *ports[MAX_PORTS];
	jack_client_t *client;
	jack_ringbuffer_t *ringbuffer[MAX_PORTS];

	bool shutdown;

	/**
	 * While this flag is set, the "process" callback generates
	 * silence.
	 */
	bool pause;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
jack_output_quark(void)
{
	return g_quark_from_static_string("jack_output");
}

static int
mpd_jack_process(jack_nframes_t nframes, void *arg)
{
	struct jack_data *jd = (struct jack_data *) arg;
	jack_default_audio_sample_t *out;
	size_t available;

	if (nframes <= 0)
		return 0;

	if (jd->pause) {
		/* generate silence while MPD is paused */

		for (unsigned i = 0; i < jd->audio_format.channels; ++i) {
			out = jack_port_get_buffer(jd->ports[i], nframes);

			for (jack_nframes_t f = 0; f < nframes; ++f)
				out[f] = 0.0;
		}

		return 0;
	}

	for (unsigned i = 0; i < jd->audio_format.channels; ++i) {
		available = jack_ringbuffer_read_space(jd->ringbuffer[i]);
		assert(available % sample_size == 0);
		available /= sample_size;
		if (available > nframes)
			available = nframes;

		out = jack_port_get_buffer(jd->ports[i], nframes);
		jack_ringbuffer_read(jd->ringbuffer[i],
				     (char *)out, available * sample_size);

		while (available < nframes)
			/* ringbuffer underrun, fill with silence */
			out[available++] = 0.0;
	}

	/* generate silence for the unused source ports */

	for (unsigned i = jd->audio_format.channels;
	     i < jd->num_source_ports; ++i) {
		out = jack_port_get_buffer(jd->ports[i], nframes);

		for (jack_nframes_t f = 0; f < nframes; ++f)
			out[f] = 0.0;
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

	if (jd->num_source_ports == 1)
		audio_format->channels = 1;
	else if (audio_format->channels > jd->num_source_ports)
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

/**
 * Disconnect the JACK client.
 */
static void
mpd_jack_disconnect(struct jack_data *jd)
{
	assert(jd != NULL);
	assert(jd->client != NULL);

	jack_deactivate(jd->client);
	jack_client_close(jd->client);
	jd->client = NULL;
}

/**
 * Connect the JACK client and performs some basic setup
 * (e.g. register callbacks).
 */
static bool
mpd_jack_connect(struct jack_data *jd, GError **error_r)
{
	jack_status_t status;

	assert(jd != NULL);

	jd->shutdown = false;

	jd->client = jack_client_open(jd->name, jd->options, &status,
				      jd->server_name);
	if (jd->client == NULL) {
		g_set_error(error_r, jack_output_quark(), 0,
			    "Failed to connect to JACK server, status=%d",
			    status);
		return false;
	}

	jack_set_process_callback(jd->client, mpd_jack_process, jd);
	jack_on_shutdown(jd->client, mpd_jack_shutdown, jd);

	for (unsigned i = 0; i < jd->num_source_ports; ++i) {
		jd->ports[i] = jack_port_register(jd->client,
						  jd->source_ports[i],
						  JACK_DEFAULT_AUDIO_TYPE,
						  JackPortIsOutput, 0);
		if (jd->ports[i] == NULL) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "Cannot register output port \"%s\"",
				    jd->source_ports[i]);
			mpd_jack_disconnect(jd);
			return false;
		}
	}

	return true;
}

static bool
mpd_jack_test_default_device(void)
{
	return true;
}

static unsigned
parse_port_list(int line, const char *source, char **dest, GError **error_r)
{
	char **list = g_strsplit(source, ",", 0);
	unsigned n = 0;

	for (n = 0; list[n] != NULL; ++n) {
		if (n >= MAX_PORTS) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "too many port names in line %d",
				    line);
			return 0;
		}

		dest[n] = list[n];
	}

	g_free(list);

	if (n == 0) {
		g_set_error(error_r, jack_output_quark(), 0,
			    "at least one port name expected in line %d",
			    line);
		return 0;
	}

	return n;
}

static void *
mpd_jack_init(G_GNUC_UNUSED const struct audio_format *audio_format,
	      const struct config_param *param, GError **error_r)
{
	struct jack_data *jd;
	const char *value;

	jd = g_new(struct jack_data, 1);
	jd->options = JackNullOption;

	jd->name = config_get_block_string(param, "client_name", NULL);
	if (jd->name != NULL)
		jd->options |= JackUseExactName;
	else
		/* if there's a no configured client name, we don't
		   care about the JackUseExactName option */
		jd->name = "Music Player Daemon";

	jd->server_name = config_get_block_string(param, "server_name", NULL);
	if (jd->server_name != NULL)
		jd->options |= JackServerName;

	if (!config_get_block_bool(param, "autostart", false))
		jd->options |= JackNoStartServer;

	/* configure the source ports */

	value = config_get_block_string(param, "source_ports", "left,right");
	jd->num_source_ports = parse_port_list(param->line, value,
					       jd->source_ports, error_r);
	if (jd->num_source_ports == 0)
		return NULL;

	/* configure the destination ports */

	value = config_get_block_string(param, "destination_ports", NULL);
	if (value == NULL) {
		/* compatibility with MPD < 0.16 */
		value = config_get_block_string(param, "ports", NULL);
		if (value != NULL)
			g_warning("deprecated option 'ports' in line %d",
				  param->line);
	}

	if (value != NULL) {
		jd->num_destination_ports =
			parse_port_list(param->line, value,
					jd->destination_ports, error_r);
		if (jd->num_destination_ports == 0)
			return NULL;
	} else {
		jd->num_destination_ports = 0;
	}

	if (jd->num_destination_ports > 0 &&
	    jd->num_destination_ports != jd->num_source_ports)
		g_warning("number of source ports (%u) mismatches the "
			  "number of destination ports (%u) in line %d",
			  jd->num_source_ports, jd->num_destination_ports,
			  param->line);

	jd->ringbuffer_size =
		config_get_block_unsigned(param, "ringbuffer_size", 32768);

	jack_set_error_function(mpd_jack_error);

#ifdef HAVE_JACK_SET_INFO_FUNCTION
	jack_set_info_function(mpd_jack_info);
#endif

	return jd;
}

static void
mpd_jack_finish(void *data)
{
	struct jack_data *jd = data;

	for (unsigned i = 0; i < jd->num_source_ports; ++i)
		g_free(jd->source_ports[i]);

	for (unsigned i = 0; i < jd->num_destination_ports; ++i)
		g_free(jd->destination_ports[i]);

	g_free(jd);
}

static bool
mpd_jack_enable(void *data, GError **error_r)
{
	struct jack_data *jd = (struct jack_data *)data;

	for (unsigned i = 0; i < jd->num_source_ports; ++i)
		jd->ringbuffer[i] = NULL;

	return mpd_jack_connect(jd, error_r);
}

static void
mpd_jack_disable(void *data)
{
	struct jack_data *jd = (struct jack_data *)data;

	if (jd->client != NULL)
		mpd_jack_disconnect(jd);

	for (unsigned i = 0; i < jd->num_source_ports; ++i) {
		if (jd->ringbuffer[i] != NULL) {
			jack_ringbuffer_free(jd->ringbuffer[i]);
			jd->ringbuffer[i] = NULL;
		}
	}
}

/**
 * Stops the playback on the JACK connection.
 */
static void
mpd_jack_stop(struct jack_data *jd)
{
	assert(jd != NULL);

	if (jd->client == NULL)
		return;

	if (jd->shutdown)
		/* the connection has failed; close it */
		mpd_jack_disconnect(jd);
	else
		/* the connection is alive: just stop playback */
		jack_deactivate(jd->client);
}

static bool
mpd_jack_start(struct jack_data *jd, GError **error_r)
{
	const char *destination_ports[MAX_PORTS], **jports;
	const char *duplicate_port = NULL;
	unsigned num_destination_ports;

	assert(jd->client != NULL);
	assert(jd->audio_format.channels <= jd->num_source_ports);

	/* allocate the ring buffers on the first open(); these
	   persist until MPD exits.  It's too unsafe to delete them
	   because we can never know when mpd_jack_process() gets
	   called */
	for (unsigned i = 0; i < jd->num_source_ports; ++i) {
		if (jd->ringbuffer[i] == NULL)
			jd->ringbuffer[i] =
				jack_ringbuffer_create(jd->ringbuffer_size);

		/* clear the ring buffer to be sure that data from
		   previous playbacks are gone */
		jack_ringbuffer_reset(jd->ringbuffer[i]);
	}

	if ( jack_activate(jd->client) ) {
		g_set_error(error_r, jack_output_quark(), 0,
			    "cannot activate client");
		mpd_jack_stop(jd);
		return false;
	}

	if (jd->num_destination_ports == 0) {
		/* no output ports were configured - ask libjack for
		   defaults */
		jports = jack_get_ports(jd->client, NULL, NULL,
					JackPortIsPhysical | JackPortIsInput);
		if (jports == NULL) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "no ports found");
			mpd_jack_stop(jd);
			return false;
		}

		assert(*jports != NULL);

		for (num_destination_ports = 0;
		     num_destination_ports < MAX_PORTS &&
			     jports[num_destination_ports] != NULL;
		     ++num_destination_ports) {
			g_debug("destination_port[%u] = '%s'\n",
				num_destination_ports,
				jports[num_destination_ports]);
			destination_ports[num_destination_ports] =
				jports[num_destination_ports];
		}
	} else {
		/* use the configured output ports */

		num_destination_ports = jd->num_destination_ports;
		memcpy(destination_ports, jd->destination_ports,
		       num_destination_ports * sizeof(*destination_ports));

		jports = NULL;
	}

	assert(num_destination_ports > 0);

	if (jd->audio_format.channels >= 2 && num_destination_ports == 1) {
		/* mix stereo signal on one speaker */

		while (num_destination_ports < jd->audio_format.channels)
			destination_ports[num_destination_ports++] =
				destination_ports[0];
	} else if (num_destination_ports > jd->audio_format.channels) {
		if (jd->audio_format.channels == 1 && num_destination_ports > 2) {
			/* mono input file: connect the one source
			   channel to the both destination channels */
			duplicate_port = destination_ports[1];
			num_destination_ports = 1;
		} else
			/* connect only as many ports as we need */
			num_destination_ports = jd->audio_format.channels;
	}

	assert(num_destination_ports <= jd->num_source_ports);

	for (unsigned i = 0; i < num_destination_ports; ++i) {
		int ret;

		ret = jack_connect(jd->client, jack_port_name(jd->ports[i]),
				   destination_ports[i]);
		if (ret != 0) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "Not a valid JACK port: %s",
				    destination_ports[i]);

			if (jports != NULL)
				free(jports);

			mpd_jack_stop(jd);
			return false;
		}
	}

	if (duplicate_port != NULL) {
		/* mono input file: connect the one source channel to
		   the both destination channels */
		int ret;

		ret = jack_connect(jd->client, jack_port_name(jd->ports[0]),
				   duplicate_port);
		if (ret != 0) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "Not a valid JACK port: %s",
				    duplicate_port);

			if (jports != NULL)
				free(jports);

			mpd_jack_stop(jd);
			return false;
		}
	}

	if (jports != NULL)
		free(jports);

	return true;
}

static bool
mpd_jack_open(void *data, struct audio_format *audio_format, GError **error_r)
{
	struct jack_data *jd = data;

	assert(jd != NULL);

	jd->pause = false;

	if (jd->client == NULL && !mpd_jack_connect(jd, error_r))
		return false;

	set_audioformat(jd, audio_format);
	jd->audio_format = *audio_format;

	if (!mpd_jack_start(jd, error_r))
		return false;

	return true;
}

static void
mpd_jack_close(G_GNUC_UNUSED void *data)
{
	struct jack_data *jd = data;

	mpd_jack_stop(jd);
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
	unsigned i;

	while (num_samples-- > 0) {
		for (i = 0; i < jd->audio_format.channels; ++i) {
			sample = sample_16_to_jack(*src++);
			jack_ringbuffer_write(jd->ringbuffer[i], (void*)&sample,
					      sizeof(sample));
		}
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
	unsigned i;

	while (num_samples-- > 0) {
		for (i = 0; i < jd->audio_format.channels; ++i) {
			sample = sample_24_to_jack(*src++);
			jack_ringbuffer_write(jd->ringbuffer[i], (void*)&sample,
					      sizeof(sample));
		}
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
mpd_jack_play(void *data, const void *chunk, size_t size, GError **error_r)
{
	struct jack_data *jd = data;
	const size_t frame_size = audio_format_frame_size(&jd->audio_format);
	size_t space = 0, space1;

	jd->pause = false;

	assert(size % frame_size == 0);
	size /= frame_size;

	while (true) {
		if (jd->shutdown) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "Refusing to play, because "
				    "there is no client thread");
			return 0;
		}

		space = jack_ringbuffer_write_space(jd->ringbuffer[0]);
		for (unsigned i = 1; i < jd->audio_format.channels; ++i) {
			space1 = jack_ringbuffer_write_space(jd->ringbuffer[i]);
			if (space > space1)
				/* send data symmetrically */
				space = space1;
		}

		if (space >= frame_size)
			break;

		/* XXX do something more intelligent to
		   synchronize */
		g_usleep(1000);
	}

	space /= sample_size;
	if (space < size)
		size = space;

	mpd_jack_write_samples(jd, chunk, size);
	return size * frame_size;
}

static bool
mpd_jack_pause(void *data)
{
	struct jack_data *jd = data;

	if (jd->shutdown)
		return false;

	jd->pause = true;

	/* due to a MPD API limitation, we have to sleep a little bit
	   here, to avoid hogging the CPU */
	g_usleep(50000);

	return true;
}

const struct audio_output_plugin jack_output_plugin = {
	.name = "jack",
	.test_default_device = mpd_jack_test_default_device,
	.init = mpd_jack_init,
	.finish = mpd_jack_finish,
	.enable = mpd_jack_enable,
	.disable = mpd_jack_disable,
	.open = mpd_jack_open,
	.play = mpd_jack_play,
	.pause = mpd_jack_pause,
	.close = mpd_jack_close,
};
