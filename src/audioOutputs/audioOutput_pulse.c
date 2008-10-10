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

#ifdef HAVE_PULSE

#include "../utils.h"
#include "../log.h"

#include <pulse/simple.h>
#include <pulse/error.h>

#define MPD_PULSE_NAME "mpd"
#define CONN_ATTEMPT_INTERVAL 60

typedef struct _PulseData {
	struct audio_output *ao;

	pa_simple *s;
	char *server;
	char *sink;
	int connAttempts;
	time_t lastAttempt;
} PulseData;

static PulseData *newPulseData(void)
{
	PulseData *ret;

	ret = xmalloc(sizeof(PulseData));

	ret->s = NULL;
	ret->server = NULL;
	ret->sink = NULL;
	ret->connAttempts = 0;
	ret->lastAttempt = 0;

	return ret;
}

static void freePulseData(PulseData * pd)
{
	if (pd->server)
		free(pd->server);
	if (pd->sink)
		free(pd->sink);
	free(pd);
}

static void *pulse_initDriver(struct audio_output *ao,
			      mpd_unused const struct audio_format *audio_format,
			      ConfigParam * param)
{
	BlockParam *server = NULL;
	BlockParam *sink = NULL;
	PulseData *pd;

	if (param) {
		server = getBlockParam(param, "server");
		sink = getBlockParam(param, "sink");
	}

	pd = newPulseData();
	pd->ao = ao;
	pd->server = server ? xstrdup(server->value) : NULL;
	pd->sink = sink ? xstrdup(sink->value) : NULL;

	return pd;
}

static void pulse_finishDriver(void *data)
{
	PulseData *pd = data;

	freePulseData(pd);
}

static int pulse_testDefault(void)
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
		WARNING("Cannot connect to default PulseAudio server: %s\n",
			pa_strerror(error));
		return -1;
	}

	pa_simple_free(s);

	return 0;
}

static int pulse_openDevice(void *data,
			    struct audio_format *audioFormat)
{
	PulseData *pd = data;
	pa_sample_spec ss;
	time_t t;
	int error;

	t = time(NULL);

	if (pd->connAttempts != 0 &&
	    (t - pd->lastAttempt) < CONN_ATTEMPT_INTERVAL)
		return -1;

	pd->connAttempts++;
	pd->lastAttempt = t;

	if (audioFormat->bits != 16) {
		ERROR("PulseAudio doesn't support %i bit audio\n",
		      audioFormat->bits);
		return -1;
	}

	ss.format = PA_SAMPLE_S16NE;
	ss.rate = audioFormat->sample_rate;
	ss.channels = audioFormat->channels;

	pd->s = pa_simple_new(pd->server, MPD_PULSE_NAME, PA_STREAM_PLAYBACK,
			      pd->sink, audio_output_get_name(pd->ao),
			      &ss, NULL, NULL,
			      &error);
	if (!pd->s) {
		ERROR("Cannot connect to server in PulseAudio output "
		      "\"%s\" (attempt %i): %s\n",
		      audio_output_get_name(pd->ao),
		      pd->connAttempts, pa_strerror(error));
		return -1;
	}

	pd->connAttempts = 0;

	DEBUG("PulseAudio output \"%s\" connected and playing %i bit, %i "
	      "channel audio at %i Hz\n",
	      audio_output_get_name(pd->ao),
	      audioFormat->bits,
	      audioFormat->channels, audioFormat->sample_rate);

	return 0;
}

static void pulse_dropBufferedAudio(void *data)
{
	PulseData *pd = data;
	int error;

	if (pa_simple_flush(pd->s, &error) < 0)
		WARNING("Flush failed in PulseAudio output \"%s\": %s\n",
			audio_output_get_name(pd->ao),
			pa_strerror(error));
}

static void pulse_closeDevice(void *data)
{
	PulseData *pd = data;

	if (pd->s) {
		pa_simple_drain(pd->s, NULL);
		pa_simple_free(pd->s);
	}
}

static int pulse_playAudio(void *data,
			   const char *playChunk, size_t size)
{
	PulseData *pd = data;
	int error;

	if (pa_simple_write(pd->s, playChunk, size, &error) < 0) {
		ERROR("PulseAudio output \"%s\" disconnecting due to write "
		      "error: %s\n",
		      audio_output_get_name(pd->ao),
		      pa_strerror(error));
		pulse_closeDevice(pd);
		return -1;
	}

	return 0;
}

const struct audio_output_plugin pulsePlugin = {
	.name = "pulse",
	.test_default_device = pulse_testDefault,
	.init = pulse_initDriver,
	.finish = pulse_finishDriver,
	.open = pulse_openDevice,
	.play = pulse_playAudio,
	.cancel = pulse_dropBufferedAudio,
	.close = pulse_closeDevice,
};

#else /* HAVE_PULSE */

DISABLED_AUDIO_OUTPUT_PLUGIN(pulsePlugin)
#endif /* HAVE_PULSE */
