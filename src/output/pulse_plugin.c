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
#include "mixer_list.h"

#include <glib.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#define MPD_PULSE_NAME "mpd"

struct pulse_data {
	const char *name;

	pa_simple *s;
	char *server;
	char *sink;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
pulse_output_quark(void)
{
	return g_quark_from_static_string("pulse_output");
}

static struct pulse_data *pulse_new_data(void)
{
	struct pulse_data *ret;

	ret = g_new(struct pulse_data, 1);

	ret->server = NULL;
	ret->sink = NULL;

	return ret;
}

static void pulse_free_data(struct pulse_data *pd)
{
	g_free(pd->server);
	g_free(pd->sink);
	g_free(pd);
}

static void *
pulse_init(G_GNUC_UNUSED const struct audio_format *audio_format,
	   const struct config_param *param, G_GNUC_UNUSED GError **error)
{
	struct pulse_data *pd;

	pd = pulse_new_data();
	pd->name = config_get_block_string(param, "name", "mpd_pulse");
	pd->server = param != NULL
		? config_dup_block_string(param, "server", NULL) : NULL;
	pd->sink = param != NULL
		? config_dup_block_string(param, "sink", NULL) : NULL;

	return pd;
}

static void pulse_finish(void *data)
{
	struct pulse_data *pd = data;

	pulse_free_data(pd);
}

static bool pulse_test_default_device(void)
{
	pa_simple *s;
	pa_sample_spec ss;
	int error;

	ss.format = PA_SAMPLE_S16NE;
	ss.rate = 44100;
	ss.channels = 2;

	s = pa_simple_new(NULL, MPD_PULSE_NAME, PA_STREAM_PLAYBACK, NULL,
			  MPD_PULSE_NAME, &ss, NULL, NULL, &error);
	if (!s) {
		g_message("Cannot connect to default PulseAudio server: %s\n",
			  pa_strerror(error));
		return false;
	}

	pa_simple_free(s);

	return true;
}

static bool
pulse_open(void *data, struct audio_format *audio_format, GError **error_r)
{
	struct pulse_data *pd = data;
	pa_sample_spec ss;
	int error;

	/* MPD doesn't support the other pulseaudio sample formats, so
	   we just force MPD to send us everything as 16 bit */
	audio_format->bits = 16;

	ss.format = PA_SAMPLE_S16NE;
	ss.rate = audio_format->sample_rate;
	ss.channels = audio_format->channels;

	pd->s = pa_simple_new(pd->server, MPD_PULSE_NAME, PA_STREAM_PLAYBACK,
			      pd->sink, pd->name,
			      &ss, NULL, NULL,
			      &error);
	if (!pd->s) {
		g_set_error(error_r, pulse_output_quark(), error,
			    "Cannot connect to PulseAudio server: %s",
			    pa_strerror(error));
		return false;
	}

	return true;
}

static void pulse_cancel(void *data)
{
	struct pulse_data *pd = data;
	int error;

	if (pa_simple_flush(pd->s, &error) < 0)
		g_warning("Flush failed in PulseAudio output \"%s\": %s\n",
			  pd->name, pa_strerror(error));
}

static void pulse_close(void *data)
{
	struct pulse_data *pd = data;

	pa_simple_drain(pd->s, NULL);
	pa_simple_free(pd->s);
}

static size_t
pulse_play(void *data, const void *chunk, size_t size, GError **error_r)
{
	struct pulse_data *pd = data;
	int error;

	if (pa_simple_write(pd->s, chunk, size, &error) < 0) {
		g_set_error(error_r, pulse_output_quark(), error,
			    "%s", pa_strerror(error));
		return 0;
	}

	return size;
}

const struct audio_output_plugin pulse_plugin = {
	.name = "pulse",
	.test_default_device = pulse_test_default_device,
	.init = pulse_init,
	.finish = pulse_finish,
	.open = pulse_open,
	.play = pulse_play,
	.cancel = pulse_cancel,
	.close = pulse_close,
	.mixer_plugin = &pulse_mixer,
};
