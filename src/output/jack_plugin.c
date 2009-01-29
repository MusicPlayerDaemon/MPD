/* jack plug in for the Music Player Daemon (MPD)
 * (c)2006 by anarch(anarchsss@gmail.com)
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
#include "../utils.h"

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

static const size_t sample_size = sizeof(jack_default_audio_sample_t);

static const char *const port_names[2] = {
	"left", "right",
};

struct jack_data {
	struct audio_output *ao;

	/* configuration */
	char *output_ports[2];
	int ringbuffer_size;

	/* for srate() only */
	struct audio_format *audio_format;

	/* jack library stuff */
	jack_port_t *ports[2];
	jack_client_t *client;
	jack_ringbuffer_t *ringbuffer[2];

	bool shutdown;
};

static const char *
mpd_jack_name(const struct jack_data *jd)
{
	return audio_output_get_name(jd->ao);
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

	mpd_jack_client_free(jd);

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
mpd_jack_srate(G_GNUC_UNUSED jack_nframes_t rate, void *data)
{
	struct jack_data *jd = (struct jack_data *)data;
	struct audio_format *audioFormat = jd->audio_format;

	audioFormat->sample_rate = (int)jack_get_sample_rate(jd->client);

	return 0;
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
	g_debug("samplerate = %u", audio_format->sample_rate);
	audio_format->channels = 2;

	if (audio_format->bits != 16 && audio_format->bits != 24)
		audio_format->bits = 24;
}

static void
mpd_jack_error(const char *msg)
{
	g_warning("%s", msg);
}

static void *
mpd_jack_init(struct audio_output *ao,
	      G_GNUC_UNUSED const struct audio_format *audio_format,
	      const struct config_param *param)
{
	struct jack_data *jd;
	const char *value;

	jd = g_new(struct jack_data, 1);
	jd->ao = ao;

	g_debug("mpd_jack_init (pid=%d)", getpid());

	value = config_get_block_string(param, "ports", NULL);
	if (value != NULL) {
		char **ports = g_strsplit(value, ",", 0);

		if (ports[0] == NULL || ports[1] == NULL || ports[2] != NULL)
			g_error("two port names expected in line %d",
				param->line);

		jd->output_ports[0] = ports[0];
		jd->output_ports[1] = ports[1];

		g_free(ports);
	}

	jd->ringbuffer_size =
		config_get_block_unsigned(param, "ringbuffer_size", 32768);

	jack_set_error_function(mpd_jack_error);

	return jd;
}

static bool
mpd_jack_test_default_device(void)
{
	return true;
}

static bool
mpd_jack_connect(struct jack_data *jd, struct audio_format *audio_format)
{
	jd->audio_format = audio_format;

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ringbuffer); ++i)
		jd->ringbuffer[i] =
			jack_ringbuffer_create(jd->ringbuffer_size);

	jd->shutdown = false;

	if ((jd->client = jack_client_new(mpd_jack_name(jd))) == NULL) {
		g_warning("jack server not running?");
		return false;
	}

	jack_set_process_callback(jd->client, mpd_jack_process, jd);
	jack_set_sample_rate_callback(jd->client, mpd_jack_srate, jd);
	jack_on_shutdown(jd->client, mpd_jack_shutdown, jd);

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ports); ++i) {
		jd->ports[i] = jack_port_register(jd->client, port_names[i],
						  JACK_DEFAULT_AUDIO_TYPE,
						  JackPortIsOutput, 0);
		if (jd->ports[i] == NULL) {
			g_warning("Cannot register %s output port.",
				  port_names[i]);
			return false;
		}
	}

	if ( jack_activate(jd->client) ) {
		g_warning("cannot activate client");
		return false;
	}

	if (jd->output_ports[1] == NULL) {
		/* no output ports were configured - ask libjack for
		   defaults */
		const char **jports;

		jports = jack_get_ports(jd->client, NULL, NULL,
					JackPortIsPhysical | JackPortIsInput);
		if (jports == NULL) {
			g_warning("no ports found");
			return false;
		}

		jd->output_ports[0] = g_strdup(jports[0]);
		jd->output_ports[1] = g_strdup(jports[1] != NULL
					       ? jports[1] : jports[0]);
		g_debug("output_ports: %s %s",
		      jd->output_ports[0], jd->output_ports[1]);
		free(jports);
	}

	for (unsigned i = 0; i < G_N_ELEMENTS(jd->ports); ++i) {
		int ret;

		ret = jack_connect(jd->client, jack_port_name(jd->ports[i]),
				   jd->output_ports[i]);
		if (ret != 0) {
			g_warning("%s is not a valid Jack Client / Port",
				  jd->output_ports[i]);
			return false;
		}
	}

	return true;
}

static bool
mpd_jack_open(void *data, struct audio_format *audio_format)
{
	struct jack_data *jd = data;

	assert(jd != NULL);

	if (jd->client == NULL && !mpd_jack_connect(jd, audio_format)) {
		mpd_jack_client_free(jd);
		return false;
	}

	set_audioformat(jd, audio_format);

	return true;
}

static void
mpd_jack_close(G_GNUC_UNUSED void *data)
{
	/*mpd_jack_finish(audioOutput);*/
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
	switch (jd->audio_format->bits) {
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

static bool
mpd_jack_play(void *data, const char *buff, size_t size)
{
	struct jack_data *jd = data;
	const size_t frame_size = audio_format_frame_size(jd->audio_format);
	size_t space, space1;

	if (jd->shutdown) {
		g_warning("Refusing to play, because there is no client thread.");
		mpd_jack_client_free(jd);
		audio_output_closed(jd->ao);
		return true;
	}

	assert(size % frame_size == 0);
	size /= frame_size;
	while (size > 0 && !jd->shutdown) {
		space = jack_ringbuffer_write_space(jd->ringbuffer[0]);
		space1 = jack_ringbuffer_write_space(jd->ringbuffer[1]);
		if (space > space1)
			/* send data symmetrically */
			space = space1;

		space /= sample_size;
		if (space > 0) {
			if (space > size)
				space = size;

			mpd_jack_write_samples(jd, buff, space);

			buff += space * frame_size;
			size -= space;
		} else {
			/* XXX do something more intelligent to
			   synchronize */
			my_usleep(1000);
		}

	}

	return true;
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
