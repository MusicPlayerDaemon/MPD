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
#include "../log.h"

#include <assert.h>

#include <glib.h>
#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>

static const size_t sample_size = sizeof(jack_default_audio_sample_t);

struct jack_data {
	struct audio_output *ao;

	/* configuration */
	char *name;
	char *output_ports[2];
	int ringbuffer_size;

	/* for srate() only */
	struct audio_format *audio_format;

	/* jack library stuff */
	jack_port_t *ports[2];
	jack_client_t *client;
	jack_ringbuffer_t *ringbuffer[2];
	int bps;
	int shutdown;
};

static const char *
mpd_jack_name(const struct jack_data *jd)
{
	return jd->name != NULL ? jd->name : "mpd";
}

static struct jack_data *
mpd_jack_new(void)
{
	struct jack_data *ret;

	ret = xcalloc(sizeof(*ret), 1);

	ret->ringbuffer_size = 32768;

	return ret;
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

	if (jd->ringbuffer[0] != NULL) {
		jack_ringbuffer_free(jd->ringbuffer[0]);
		jd->ringbuffer[0] = NULL;
	}

	if (jd->ringbuffer[1] != NULL) {
		jack_ringbuffer_free(jd->ringbuffer[1]);
		jd->ringbuffer[1] = NULL;
	}
}

static void
mpd_jack_free(struct jack_data *jd)
{
	int i;

	assert(jd != NULL);

	mpd_jack_client_free(jd);

	g_free(jd->name);

	for ( i = ARRAY_SIZE(jd->output_ports); --i >= 0; ) {
		if (!jd->output_ports[i])
			continue;
		xfree(jd->output_ports[i]);
	}

	free(jd);
}

static void
mpd_jack_finish(void *data)
{
	struct jack_data *jd = data;
	mpd_jack_free(jd);
}

static int
mpd_jack_srate(mpd_unused jack_nframes_t rate, void *data)
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

	for (unsigned i = 0; i < 2; ++i) {
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
	jd->shutdown = 1;
}

static void
set_audioformat(struct jack_data *jd, struct audio_format *audio_format)
{
	audio_format->sample_rate = jack_get_sample_rate(jd->client);
	DEBUG("samplerate = %u\n", audio_format->sample_rate);
	audio_format->channels = 2;

	if (audio_format->bits != 16 && audio_format->bits != 24)
		audio_format->bits = 24;

	jd->bps = audio_format->channels
		* sizeof(jack_default_audio_sample_t)
		* audio_format->sample_rate;
}

static void
mpd_jack_error(const char *msg)
{
	ERROR("jack: %s\n", msg);
}

static void *
mpd_jack_init(struct audio_output *ao,
	      mpd_unused const struct audio_format *audio_format,
	      ConfigParam *param)
{
	struct jack_data *jd;
	BlockParam *bp;
	char *endptr;
	int val;
	char *cp = NULL;

	jd = mpd_jack_new();
	jd->ao = ao;

	DEBUG("mpd_jack_init (pid=%d)\n", getpid());
	if (param == NULL)
		return jd;

	if ( (bp = getBlockParam(param, "ports")) ) {
		DEBUG("output_ports=%s\n", bp->value);

		if (!(cp = strchr(bp->value, ',')))
			FATAL("expected comma and a second value for '%s' "
			      "at line %d: %s\n",
			      bp->name, bp->line, bp->value);

		*cp = '\0';
		jd->output_ports[0] = xstrdup(bp->value);
		*cp++ = ',';

		if (!*cp)
			FATAL("expected a second value for '%s' at line %d: "
			      "%s\n", bp->name, bp->line, bp->value);

		jd->output_ports[1] = xstrdup(cp);

		if (strchr(cp,','))
			FATAL("Only %d values are supported for '%s' "
			      "at line %d\n",
			      (int)ARRAY_SIZE(jd->output_ports),
			      bp->name, bp->line);
	}

	if ( (bp = getBlockParam(param, "ringbuffer_size")) ) {
		errno = 0;
		val = strtol(bp->value, &endptr, 10);

		if ( errno == 0 && endptr != bp->value) {
			jd->ringbuffer_size = val < 32768 ? 32768 : val;
			DEBUG("ringbuffer_size=%d\n", jd->ringbuffer_size);
		} else {
			FATAL("%s is not a number; ringbuf_size=%d\n",
			      bp->value, jd->ringbuffer_size);
		}
	}

	if ( (bp = getBlockParam(param, "name"))
	     && (strcmp(bp->value, "mpd") != 0) ) {
		jd->name = xstrdup(bp->value);
		DEBUG("name=%s\n", jd->name);
	} else
		jd->name = NULL;

	return jd;
}

static bool
mpd_jack_test_default_device(void)
{
	return true;
}

static int
mpd_jack_connect(struct jack_data *jd, struct audio_format *audio_format)
{
	const char **jports;
	char *port_name;

	jd->audio_format = audio_format;

	jd->ringbuffer[0] = jack_ringbuffer_create(jd->ringbuffer_size);
	jd->ringbuffer[1] = jack_ringbuffer_create(jd->ringbuffer_size);

	jd->shutdown = false;

	if ((jd->client = jack_client_new(mpd_jack_name(jd))) == NULL) {
		ERROR("jack server not running?\n");
		return -1;
	}

	jack_set_error_function(mpd_jack_error);
	jack_set_process_callback(jd->client, mpd_jack_process, jd);
	jack_set_sample_rate_callback(jd->client, mpd_jack_srate, jd);
	jack_on_shutdown(jd->client, mpd_jack_shutdown, jd);

	jd->ports[0] = jack_port_register(jd->client, "left",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
	if ( !jd->ports[0] ) {
		ERROR("Cannot register left output port.\n");
		return -1;
	}

	jd->ports[1] = jack_port_register(jd->client, "right",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
	if ( !jd->ports[1] ) {
		ERROR("Cannot register right output port.\n");
		return -1;
	}

	if ( jack_activate(jd->client) ) {
		ERROR("cannot activate client\n");
		return -1;
	}

	/*  hay que buscar que hay  */
	if (!jd->output_ports[1] &&
	    (jports = jack_get_ports(jd->client, NULL, NULL,
				     JackPortIsPhysical | JackPortIsInput))) {
		jd->output_ports[0] = g_strdup(jports[0]);
		jd->output_ports[1] = g_strdup(jports[1] != NULL
					       ? jports[1] : jports[0]);
		DEBUG("output_ports: %s %s\n",
		      jd->output_ports[0], jd->output_ports[1]);
		free(jports);
	}

	if ( jd->output_ports[1] ) {
		const char *name = mpd_jack_name(jd);

		port_name = xmalloc(sizeof(char) * (7 + strlen(name)));

		sprintf(port_name, "%s:left", name);
		if ( (jack_connect(jd->client, port_name,
				   jd->output_ports[0])) != 0 ) {
			ERROR("%s is not a valid Jack Client / Port\n",
			      jd->output_ports[0]);
			free(port_name);
			return -1;
		}
		sprintf(port_name, "%s:right", name);
		if ( (jack_connect(jd->client, port_name,
				   jd->output_ports[1])) != 0 ) {
			ERROR("%s is not a valid Jack Client / Port\n",
			      jd->output_ports[1]);
			free(port_name);
			return -1;
		}
		free(port_name);
	}

	return 1;
}

static bool
mpd_jack_open(void *data, struct audio_format *audio_format)
{
	struct jack_data *jd = data;

	assert(jd != NULL);

	if (jd->client == NULL && mpd_jack_connect(jd, audio_format) < 0) {
		mpd_jack_client_free(jd);
		return false;
	}

	set_audioformat(jd, audio_format);

	return true;
}

static void
mpd_jack_close(mpd_unused void *data)
{
	/*mpd_jack_finish(audioOutput);*/
}

static void
mpd_jack_cancel (mpd_unused void *data)
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
		ERROR("Refusing to play, because there is no client thread.\n");
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
			my_usleep(10000);
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
