/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "JackOutputPlugin.hxx"
#include "OutputAPI.hxx"

#include <assert.h>

#include <glib.h>
#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "jack"

enum {
	MAX_PORTS = 16,
};

static const size_t jack_sample_size = sizeof(jack_default_audio_sample_t);

struct JackOutput {
	struct audio_output base;

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

	bool Initialize(const config_param *param, GError **error_r) {
		return ao_base_init(&base, &jack_output_plugin, param,
				    error_r);
	}

	void Deinitialize() {
		ao_base_finish(&base);
	}
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
jack_output_quark(void)
{
	return g_quark_from_static_string("jack_output");
}

/**
 * Determine the number of frames guaranteed to be available on all
 * channels.
 */
static jack_nframes_t
mpd_jack_available(const JackOutput *jd)
{
	size_t min = jack_ringbuffer_read_space(jd->ringbuffer[0]);

	for (unsigned i = 1; i < jd->audio_format.channels; ++i) {
		size_t current = jack_ringbuffer_read_space(jd->ringbuffer[i]);
		if (current < min)
			min = current;
	}

	assert(min % jack_sample_size == 0);

	return min / jack_sample_size;
}

static int
mpd_jack_process(jack_nframes_t nframes, void *arg)
{
	JackOutput *jd = (JackOutput *) arg;

	if (nframes <= 0)
		return 0;

	if (jd->pause) {
		/* empty the ring buffers */

		const jack_nframes_t available = mpd_jack_available(jd);
		for (unsigned i = 0; i < jd->audio_format.channels; ++i)
			jack_ringbuffer_read_advance(jd->ringbuffer[i],
						     available * jack_sample_size);

		/* generate silence while MPD is paused */

		for (unsigned i = 0; i < jd->audio_format.channels; ++i) {
			jack_default_audio_sample_t *out =
				(jack_default_audio_sample_t *)
				jack_port_get_buffer(jd->ports[i], nframes);

			for (jack_nframes_t f = 0; f < nframes; ++f)
				out[f] = 0.0;
		}

		return 0;
	}

	jack_nframes_t available = mpd_jack_available(jd);
	if (available > nframes)
		available = nframes;

	for (unsigned i = 0; i < jd->audio_format.channels; ++i) {
		jack_default_audio_sample_t *out =
			(jack_default_audio_sample_t *)
			jack_port_get_buffer(jd->ports[i], nframes);
		if (out == nullptr)
			/* workaround for libjack1 bug: if the server
			   connection fails, the process callback is
			   invoked anyway, but unable to get a
			   buffer */
			continue;

		jack_ringbuffer_read(jd->ringbuffer[i],
				     (char *)out, available * jack_sample_size);

		for (jack_nframes_t f = available; f < nframes; ++f)
			/* ringbuffer underrun, fill with silence */
			out[f] = 0.0;
	}

	/* generate silence for the unused source ports */

	for (unsigned i = jd->audio_format.channels;
	     i < jd->num_source_ports; ++i) {
		jack_default_audio_sample_t *out =
			(jack_default_audio_sample_t *)
			jack_port_get_buffer(jd->ports[i], nframes);
		if (out == nullptr)
			/* workaround for libjack1 bug: if the server
			   connection fails, the process callback is
			   invoked anyway, but unable to get a
			   buffer */
			continue;

		for (jack_nframes_t f = 0; f < nframes; ++f)
			out[f] = 0.0;
	}

	return 0;
}

static void
mpd_jack_shutdown(void *arg)
{
	JackOutput *jd = (JackOutput *) arg;
	jd->shutdown = true;
}

static void
set_audioformat(JackOutput *jd, struct audio_format *audio_format)
{
	audio_format->sample_rate = jack_get_sample_rate(jd->client);

	if (jd->num_source_ports == 1)
		audio_format->channels = 1;
	else if (audio_format->channels > jd->num_source_ports)
		audio_format->channels = 2;

	if (audio_format->format != SAMPLE_FORMAT_S16 &&
	    audio_format->format != SAMPLE_FORMAT_S24_P32)
		audio_format->format = SAMPLE_FORMAT_S24_P32;
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
mpd_jack_disconnect(JackOutput *jd)
{
	assert(jd != nullptr);
	assert(jd->client != nullptr);

	jack_deactivate(jd->client);
	jack_client_close(jd->client);
	jd->client = nullptr;
}

/**
 * Connect the JACK client and performs some basic setup
 * (e.g. register callbacks).
 */
static bool
mpd_jack_connect(JackOutput *jd, GError **error_r)
{
	jack_status_t status;

	assert(jd != nullptr);

	jd->shutdown = false;

	jd->client = jack_client_open(jd->name, jd->options, &status,
				      jd->server_name);
	if (jd->client == nullptr) {
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
		if (jd->ports[i] == nullptr) {
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

	for (n = 0; list[n] != nullptr; ++n) {
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

static struct audio_output *
mpd_jack_init(const config_param *param, GError **error_r)
{
	JackOutput *jd = new JackOutput();

	if (!jd->Initialize(param, error_r)) {
		delete jd;
		return nullptr;
	}

	const char *value;

	jd->options = JackNullOption;

	jd->name = config_get_block_string(param, "client_name", nullptr);
	if (jd->name != nullptr)
		jd->options = jack_options_t(jd->options | JackUseExactName);
	else
		/* if there's a no configured client name, we don't
		   care about the JackUseExactName option */
		jd->name = "Music Player Daemon";

	jd->server_name = config_get_block_string(param, "server_name", nullptr);
	if (jd->server_name != nullptr)
		jd->options = jack_options_t(jd->options | JackServerName);

	if (!config_get_block_bool(param, "autostart", false))
		jd->options = jack_options_t(jd->options | JackNoStartServer);

	/* configure the source ports */

	value = config_get_block_string(param, "source_ports", "left,right");
	jd->num_source_ports = parse_port_list(param->line, value,
					       jd->source_ports, error_r);
	if (jd->num_source_ports == 0)
		return nullptr;

	/* configure the destination ports */

	value = config_get_block_string(param, "destination_ports", nullptr);
	if (value == nullptr) {
		/* compatibility with MPD < 0.16 */
		value = config_get_block_string(param, "ports", nullptr);
		if (value != nullptr)
			g_warning("deprecated option 'ports' in line %d",
				  param->line);
	}

	if (value != nullptr) {
		jd->num_destination_ports =
			parse_port_list(param->line, value,
					jd->destination_ports, error_r);
		if (jd->num_destination_ports == 0)
			return nullptr;
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

	return &jd->base;
}

static void
mpd_jack_finish(struct audio_output *ao)
{
	JackOutput *jd = (JackOutput *)ao;

	for (unsigned i = 0; i < jd->num_source_ports; ++i)
		g_free(jd->source_ports[i]);

	for (unsigned i = 0; i < jd->num_destination_ports; ++i)
		g_free(jd->destination_ports[i]);

	jd->Deinitialize();
	delete jd;
}

static bool
mpd_jack_enable(struct audio_output *ao, GError **error_r)
{
	JackOutput *jd = (JackOutput *)ao;

	for (unsigned i = 0; i < jd->num_source_ports; ++i)
		jd->ringbuffer[i] = nullptr;

	return mpd_jack_connect(jd, error_r);
}

static void
mpd_jack_disable(struct audio_output *ao)
{
	JackOutput *jd = (JackOutput *)ao;

	if (jd->client != nullptr)
		mpd_jack_disconnect(jd);

	for (unsigned i = 0; i < jd->num_source_ports; ++i) {
		if (jd->ringbuffer[i] != nullptr) {
			jack_ringbuffer_free(jd->ringbuffer[i]);
			jd->ringbuffer[i] = nullptr;
		}
	}
}

/**
 * Stops the playback on the JACK connection.
 */
static void
mpd_jack_stop(JackOutput *jd)
{
	assert(jd != nullptr);

	if (jd->client == nullptr)
		return;

	if (jd->shutdown)
		/* the connection has failed; close it */
		mpd_jack_disconnect(jd);
	else
		/* the connection is alive: just stop playback */
		jack_deactivate(jd->client);
}

static bool
mpd_jack_start(JackOutput *jd, GError **error_r)
{
	const char *destination_ports[MAX_PORTS], **jports;
	const char *duplicate_port = nullptr;
	unsigned num_destination_ports;

	assert(jd->client != nullptr);
	assert(jd->audio_format.channels <= jd->num_source_ports);

	/* allocate the ring buffers on the first open(); these
	   persist until MPD exits.  It's too unsafe to delete them
	   because we can never know when mpd_jack_process() gets
	   called */
	for (unsigned i = 0; i < jd->num_source_ports; ++i) {
		if (jd->ringbuffer[i] == nullptr)
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
		jports = jack_get_ports(jd->client, nullptr, nullptr,
					JackPortIsPhysical | JackPortIsInput);
		if (jports == nullptr) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "no ports found");
			mpd_jack_stop(jd);
			return false;
		}

		assert(*jports != nullptr);

		for (num_destination_ports = 0;
		     num_destination_ports < MAX_PORTS &&
			     jports[num_destination_ports] != nullptr;
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

		jports = nullptr;
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

			if (jports != nullptr)
				free(jports);

			mpd_jack_stop(jd);
			return false;
		}
	}

	if (duplicate_port != nullptr) {
		/* mono input file: connect the one source channel to
		   the both destination channels */
		int ret;

		ret = jack_connect(jd->client, jack_port_name(jd->ports[0]),
				   duplicate_port);
		if (ret != 0) {
			g_set_error(error_r, jack_output_quark(), 0,
				    "Not a valid JACK port: %s",
				    duplicate_port);

			if (jports != nullptr)
				free(jports);

			mpd_jack_stop(jd);
			return false;
		}
	}

	if (jports != nullptr)
		free(jports);

	return true;
}

static bool
mpd_jack_open(struct audio_output *ao, struct audio_format *audio_format,
	      GError **error_r)
{
	JackOutput *jd = (JackOutput *)ao;

	assert(jd != nullptr);

	jd->pause = false;

	if (jd->client != nullptr && jd->shutdown)
		mpd_jack_disconnect(jd);

	if (jd->client == nullptr && !mpd_jack_connect(jd, error_r))
		return false;

	set_audioformat(jd, audio_format);
	jd->audio_format = *audio_format;

	if (!mpd_jack_start(jd, error_r))
		return false;

	return true;
}

static void
mpd_jack_close(G_GNUC_UNUSED struct audio_output *ao)
{
	JackOutput *jd = (JackOutput *)ao;

	mpd_jack_stop(jd);
}

static unsigned
mpd_jack_delay(struct audio_output *ao)
{
	JackOutput *jd = (JackOutput *)ao;

	return jd->base.pause && jd->pause && !jd->shutdown
		? 1000
		: 0;
}

static inline jack_default_audio_sample_t
sample_16_to_jack(int16_t sample)
{
	return sample / (jack_default_audio_sample_t)(1 << (16 - 1));
}

static void
mpd_jack_write_samples_16(JackOutput *jd, const int16_t *src,
			  unsigned num_samples)
{
	jack_default_audio_sample_t sample;
	unsigned i;

	while (num_samples-- > 0) {
		for (i = 0; i < jd->audio_format.channels; ++i) {
			sample = sample_16_to_jack(*src++);
			jack_ringbuffer_write(jd->ringbuffer[i],
					      (const char *)&sample,
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
mpd_jack_write_samples_24(JackOutput *jd, const int32_t *src,
			  unsigned num_samples)
{
	jack_default_audio_sample_t sample;
	unsigned i;

	while (num_samples-- > 0) {
		for (i = 0; i < jd->audio_format.channels; ++i) {
			sample = sample_24_to_jack(*src++);
			jack_ringbuffer_write(jd->ringbuffer[i],
					      (const char *)&sample,
					      sizeof(sample));
		}
	}
}

static void
mpd_jack_write_samples(JackOutput *jd, const void *src,
		       unsigned num_samples)
{
	switch (jd->audio_format.format) {
	case SAMPLE_FORMAT_S16:
		mpd_jack_write_samples_16(jd, (const int16_t*)src,
					  num_samples);
		break;

	case SAMPLE_FORMAT_S24_P32:
		mpd_jack_write_samples_24(jd, (const int32_t*)src,
					  num_samples);
		break;

	default:
		assert(false);
		gcc_unreachable();
	}
}

static size_t
mpd_jack_play(struct audio_output *ao, const void *chunk, size_t size,
	      GError **error_r)
{
	JackOutput *jd = (JackOutput *)ao;
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

static bool
mpd_jack_pause(struct audio_output *ao)
{
	JackOutput *jd = (JackOutput *)ao;

	if (jd->shutdown)
		return false;

	jd->pause = true;

	return true;
}

const struct audio_output_plugin jack_output_plugin = {
	"jack",
	mpd_jack_test_default_device,
	mpd_jack_init,
	mpd_jack_finish,
	mpd_jack_enable,
	mpd_jack_disable,
	mpd_jack_open,
	mpd_jack_close,
	mpd_jack_delay,
	nullptr,
	mpd_jack_play,
	nullptr,
	nullptr,
	mpd_jack_pause,
	nullptr,
};
