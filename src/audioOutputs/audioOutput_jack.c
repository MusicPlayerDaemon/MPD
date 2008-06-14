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

#include "../audioOutput.h"

#ifdef HAVE_JACK

#include <stdlib.h>
#include <errno.h>

#include "../conf.h"
#include "../log.h"

#include <string.h>
#include <pthread.h>

#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>

pthread_mutex_t play_audio_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  play_audio = PTHREAD_COND_INITIALIZER;

/*#include "dmalloc.h"*/

#define MIN(a, b) ((a) < (b) ? (a) : (b))
/*#define SAMPLE_SIZE  sizeof(jack_default_audio_sample_t);*/


static char *name = "mpd";
static char *output_ports[2];
static int ringbuf_sz = 32768;
size_t sample_size = sizeof(jack_default_audio_sample_t);

typedef struct _JackData {
	jack_port_t *ports[2];
	jack_client_t *client;
	jack_ringbuffer_t *ringbuffer[2];
	int bps;
	int shutdown;
} JackData;

/*JackData *jd = NULL;*/

static JackData *newJackData(void)
{
	JackData *ret;
	ret = xcalloc(sizeof(JackData), 1);

	return ret;
}

static void freeJackData(AudioOutput *audioOutput)
{
	JackData *jd = audioOutput->data;
	if (jd) {
		if (jd->ringbuffer[0])
			jack_ringbuffer_free(jd->ringbuffer[0]);
		if (jd->ringbuffer[1])
			jack_ringbuffer_free(jd->ringbuffer[1]);
		free(jd);
		audioOutput->data = NULL;
	}
}

static void jack_finishDriver(AudioOutput *audioOutput)
{
	JackData *jd = audioOutput->data;
	int i;

	if ( jd && jd->client ) {
		jack_deactivate(jd->client);
		jack_client_close(jd->client);
	}
	DEBUG("disconnect_jack (pid=%d)\n", getpid ());

 	if ( strcmp(name, "mpd") ) {
 		free(name);
 		name = "mpd";
 	}

	for ( i = ARRAY_SIZE(output_ports); --i >= 0; ) {
		if (!output_ports[i])
 			continue;
 		free(output_ports[i]);
 		output_ports[i] = NULL;
 	}

	freeJackData(audioOutput);
}

static int srate(jack_nframes_t rate, void *data)
{
	JackData *jd = (JackData *) ((AudioOutput*) data)->data;
 	AudioFormat *audioFormat = &(((AudioOutput*) data)->outAudioFormat);

 	audioFormat->sampleRate = (int)jack_get_sample_rate(jd->client);

	return 0;
}

static int process(jack_nframes_t nframes, void *arg)
{
	size_t i;
	JackData *jd = (JackData *) arg;
	jack_default_audio_sample_t *out[2];
	size_t avail_data, avail_frames;

	if ( nframes <= 0 )
		return 0;

	out[0] = jack_port_get_buffer(jd->ports[0], nframes);
	out[1] = jack_port_get_buffer(jd->ports[1], nframes);

	while ( nframes ) {
		avail_data = jack_ringbuffer_read_space(jd->ringbuffer[1]);

		if ( avail_data > 0 ) {
		    avail_frames = avail_data / sample_size;

		    if (avail_frames > nframes) {
			avail_frames = nframes;
			avail_data = nframes*sample_size;
		    }

		    jack_ringbuffer_read(jd->ringbuffer[0], (char *)out[0],
					 avail_data);
		    jack_ringbuffer_read(jd->ringbuffer[1], (char *)out[1],
					 avail_data);

		    nframes -= avail_frames;
		    out[0] += avail_data;
		    out[1] += avail_data;
		} else {
		    for (i = 0; i < nframes; i++)
  			out[0][i] = out[1][i] = 0.0;
		    nframes = 0;
		}

		if (pthread_mutex_trylock (&play_audio_lock) == 0) {
			pthread_cond_signal (&play_audio);
			pthread_mutex_unlock (&play_audio_lock);
		}
	}


	/*DEBUG("process (pid=%d)\n", getpid());*/
	return 0;
}

static void shutdown_callback(void *arg)
{
	JackData *jd = (JackData *) arg;
	jd->shutdown = 1;
}

static void set_audioformat(AudioOutput *audioOutput)
{
	JackData *jd = audioOutput->data;
	AudioFormat *audioFormat = &audioOutput->outAudioFormat;

	audioFormat->sampleRate = (int) jack_get_sample_rate(jd->client);
	DEBUG("samplerate = %d\n", audioFormat->sampleRate);
	audioFormat->channels = 2;
	audioFormat->bits = 16;
	jd->bps = audioFormat->channels
		* sizeof(jack_default_audio_sample_t)
		* audioFormat->sampleRate;
}

static void error_callback(const char *msg)
{
	ERROR("jack: %s\n", msg);
}

static int jack_initDriver(AudioOutput *audioOutput, ConfigParam *param)
{
	BlockParam *bp;
	char *endptr;
	int val;
	char *cp = NULL;

	audioOutput->data = NULL;

	DEBUG("jack_initDriver (pid=%d)\n", getpid());
	if ( ! param ) return 0;

	if ( (bp = getBlockParam(param, "ports")) ) {
		DEBUG("output_ports=%s\n", bp->value);

		if (!(cp = strchr(bp->value, ',')))
			FATAL("expected comma and a second value for '%s' "
			      "at line %d: %s\n",
			      bp->name, bp->line, bp->value);

		*cp = '\0';
		output_ports[0] = xstrdup(bp->value);
		*cp++ = ',';

		if (!*cp)
			FATAL("expected a second value for '%s' at line %d: "
			      "%s\n", bp->name, bp->line, bp->value);

		output_ports[1] = xstrdup(cp);

		if (strchr(cp,','))
			FATAL("Only %d values are supported for '%s' "
			      "at line %d\n", (int)ARRAY_SIZE(output_ports),
			      bp->name, bp->line);
	}

	if ( (bp = getBlockParam(param, "ringbuffer_size")) ) {
		errno = 0;
		val = strtol(bp->value, &endptr, 10);

		if ( errno == 0 && endptr != bp->value) {
			ringbuf_sz = val < 32768 ? 32768 : val;
			DEBUG("ringbuffer_size=%d\n", ringbuf_sz);
		} else {
			FATAL("%s is not a number; ringbuf_size=%d\n",
			      bp->value, ringbuf_sz);
		}
	}

	if ( (bp = getBlockParam(param, "name"))
	     && (strcmp(bp->value, "mpd") != 0) ) {
		name = xstrdup(bp->value);
		DEBUG("name=%s\n", name);
	}

 	return 0;
}

static int jack_testDefault(void)
{
	return 0;
}

static int connect_jack(AudioOutput *audioOutput)
{
	JackData *jd = audioOutput->data;
	char **jports;
	char *port_name;

	if ( (jd->client = jack_client_new(name)) == NULL ) {
		ERROR("jack server not running?\n");
		freeJackData(audioOutput);
		return -1;
	}

	jack_set_error_function(error_callback);
	jack_set_process_callback(jd->client, process, (void *)jd);
	jack_set_sample_rate_callback(jd->client, (JackProcessCallback)srate,
				      (void *)audioOutput);
	jack_on_shutdown(jd->client, shutdown_callback, (void *)jd);

	if ( jack_activate(jd->client) ) {
		ERROR("cannot activate client");
		freeJackData(audioOutput);
		return -1;
	}

	jd->ports[0] = jack_port_register(jd->client, "left",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
	if ( !jd->ports[0] ) {
		ERROR("Cannot register left output port.\n");
		freeJackData(audioOutput);
		return -1;
	}

	jd->ports[1] = jack_port_register(jd->client, "right",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
	if ( !jd->ports[1] ) {
		ERROR("Cannot register right output port.\n");
		freeJackData(audioOutput);
		return -1;
	}

	/*  hay que buscar que hay  */
	if ( !output_ports[1]
	     && (jports = (char **)jack_get_ports(jd->client, NULL, NULL,
							JackPortIsPhysical|
							JackPortIsInput)) ) {
		output_ports[0] = jports[0];
		output_ports[1] = jports[1] ? jports[1] : jports[0];
		DEBUG("output_ports: %s %s\n", output_ports[0], output_ports[1]);
		free(jports);
	}

	if ( output_ports[1] ) {
		jd->ringbuffer[0] = jack_ringbuffer_create(ringbuf_sz);
		jd->ringbuffer[1] = jack_ringbuffer_create(ringbuf_sz);
		memset(jd->ringbuffer[0]->buf, 0, jd->ringbuffer[0]->size);
		memset(jd->ringbuffer[1]->buf, 0, jd->ringbuffer[1]->size);

		port_name = xmalloc(sizeof(char)*(7+strlen(name)));

		sprintf(port_name, "%s:left", name);
		if ( (jack_connect(jd->client, port_name,
				   output_ports[0])) != 0 ) {
			ERROR("%s is not a valid Jack Client / Port ",
			      output_ports[0]);
			freeJackData(audioOutput);
			free(port_name);
			return -1;
		}
		sprintf(port_name, "%s:right", name);
		if ( (jack_connect(jd->client, port_name,
				   output_ports[1])) != 0 ) {
			ERROR("%s is not a valid Jack Client / Port ",
			      output_ports[1]);
			freeJackData(audioOutput);
			free(port_name);
			return -1;
		}
		free(port_name);
	}

	DEBUG("connect_jack (pid=%d)\n", getpid());
	return 1;
}

static int jack_openDevice(AudioOutput *audioOutput)
{
	JackData *jd = audioOutput->data;

	if ( !jd ) {
		DEBUG("connect!\n");
		jd = newJackData();
		audioOutput->data = jd;

		if (connect_jack(audioOutput) < 0) {
			freeJackData(audioOutput);
			audioOutput->open = 0;
			return -1;
		}
	}

	set_audioformat(audioOutput);
	audioOutput->open = 1;

	DEBUG("jack_openDevice (pid=%d)!\n", getpid ());
	return 0;
}


static void jack_closeDevice(AudioOutput * audioOutput)
{
	/*jack_finishDriver(audioOutput);*/
	audioOutput->open = 0;
	DEBUG("jack_closeDevice (pid=%d)\n", getpid());
}

static void jack_dropBufferedAudio (AudioOutput * audioOutput)
{
}

static int jack_playAudio(AudioOutput * audioOutput, char *buff, int size)
{
	JackData *jd = audioOutput->data;
	size_t space;
	int i;
	short *buffer = (short *) buff;
	jack_default_audio_sample_t sample;
	size_t samples = size/4;

	/*DEBUG("jack_playAudio: (pid=%d)!\n", getpid());*/

	if ( jd->shutdown ) {
		ERROR("Refusing to play, because there is no client thread.\n");
		freeJackData(audioOutput);
		audioOutput->open = 0;
		return 0;
	}

	while ( samples && !jd->shutdown ) {

 		if ( (space = jack_ringbuffer_write_space(jd->ringbuffer[0]))
 		     >= samples*sample_size ) {

			/*space = MIN(space, samples*sample_size);*/
			/*space = samples*sample_size;*/

			/*for(i=0; i<space/sample_size; i++) {*/
			for(i=0; i<samples; i++) {
				sample = (jack_default_audio_sample_t) *(buffer++)/32768.0;

				jack_ringbuffer_write(jd->ringbuffer[0], (void*)&sample,
						      sample_size);

				sample = (jack_default_audio_sample_t) *(buffer++)/32768.0;

				jack_ringbuffer_write(jd->ringbuffer[1], (void*)&sample,
						      sample_size);

				/*samples--;*/
			}
			samples=0;

 		} else {
			pthread_mutex_lock(&play_audio_lock);
			pthread_cond_wait(&play_audio, &play_audio_lock);
			pthread_mutex_unlock(&play_audio_lock);
		}

	}
	return 0;
}

AudioOutputPlugin jackPlugin = {
	"jack",
	jack_testDefault,
	jack_initDriver,
	jack_finishDriver,
	jack_openDevice,
	jack_playAudio,
	jack_dropBufferedAudio,
	jack_closeDevice,
	NULL,	/* sendMetadataFunc */
};

#else /* HAVE JACK */

DISABLED_AUDIO_OUTPUT_PLUGIN(jackPlugin)

#endif /* HAVE_JACK */
