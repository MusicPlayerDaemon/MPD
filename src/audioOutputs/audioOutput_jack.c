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
#include "../sig_handlers.h"

#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>

#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
int ringbuf_sz = 32768;
char *ports[2] = {NULL, NULL};

typedef struct _JackData {
	jack_options_t options;
	jack_port_t *ports[2];
	jack_client_t *client;
	jack_default_audio_sample_t **in;
	jack_ringbuffer_t *ringbuffer[2];
	int bps;
	int shutdown;
	int nports;
	int our_xrun;
} JackData;

JackData *jd = NULL;

static JackData *newJackData (void)
{
	JackData *ret;

	ret = calloc (sizeof (JackData), 1);
	ret->options = JackNullOption;

	return ret;
}

static void disconnect_jack (JackData *jd)
{
	jack_deactivate (jd->client);
	jack_client_close (jd->client);

	ERROR ("disconnect_jack (pid=%d)\n", getpid ());
}

static void jack_finishDriver (AudioOutput * audioOutput)
{
	JackData *jd = audioOutput->data;

	disconnect_jack (jd);

	free (jd);
	free (ports[0]);
	free (ports[1]);
}

static int srate (jack_nframes_t rate, void *data)
{
	JackData *jd = (JackData *) ((AudioOutput*) data)->data;
 	AudioFormat *audioFormat = &(((AudioOutput*) data)->outAudioFormat);

 	audioFormat->sampleRate = (int) jack_get_sample_rate (jd->client);

	return 0;
}

static int process (jack_nframes_t nframes, void *arg)
{
	size_t i;
	JackData *jd = (JackData *) arg;
	jack_default_audio_sample_t *out[2];
	size_t avail_data, avail_frames;

	if ( nframes <= 0 )
		return 0;

	out[0] = jack_port_get_buffer (jd->ports[0], nframes);
	out[1] = jack_port_get_buffer (jd->ports[1], nframes);

	avail_data = jack_ringbuffer_read_space (jd->ringbuffer[1]);

	if ( avail_data > 0 ) {
		avail_frames = avail_data / sizeof (jack_default_audio_sample_t);
		if (avail_frames > nframes) {
			avail_frames = nframes;
			avail_data = nframes * sizeof (jack_default_audio_sample_t);
		}
		jack_ringbuffer_read (jd->ringbuffer[0], (char *)out[0], avail_data);
		jack_ringbuffer_read (jd->ringbuffer[1], (char *)out[1], avail_data);

		if (avail_frames < nframes) {
			jd->our_xrun = 1;
			for (i = avail_frames; i < nframes; i++) {
				out[0][i] = out[1][i] = 0.0;
			}
		}
	} else {
		//ERROR ("avail_data=%d, no play (pid=%d)!\n", avail_data, getpid ());

 		for (i = 0; i < nframes; i++) {
 			out[0][i] = 0.0;
 			out[1][i] = 0.0;
 		}
	}

	return 0;
}

static void shutdown_callback (void *arg)
{
	JackData *jd = (JackData *) arg;
	jd->shutdown = 1;
}

static void set_audioformat (AudioOutput *audioOutput)
{
	JackData *jd = audioOutput->data;
	AudioFormat *audioFormat = &audioOutput->outAudioFormat;

	audioFormat->sampleRate = (int) jack_get_sample_rate (jd->client);
	ERROR ("samplerate = %d\n", audioFormat->sampleRate);
	jd->nports = audioFormat->channels = 2;
	audioFormat->bits = 16;
	jd->bps = audioFormat->channels *
		  audioFormat->channels *
		  audioFormat->sampleRate;
}

static int connect_jack (AudioOutput *audioOutput)
{
	JackData *jd = audioOutput->data;
	const char **jports;

	if ( (jd->client = jack_client_new ("mpd")) == NULL ) {
		ERROR ("jack server not running?\n");
		return -1;
	}

	jd->ringbuffer[0] = jack_ringbuffer_create (ringbuf_sz);
	jd->ringbuffer[1] = jack_ringbuffer_create (ringbuf_sz);

	jack_set_process_callback (jd->client, process, (void *)jd);
	jack_set_sample_rate_callback (jd->client, (JackProcessCallback)srate,
				       (void *)audioOutput);
	jack_on_shutdown (jd->client, shutdown_callback, (void *)jd);

	if ( jack_activate (jd->client) ) {
		ERROR ("cannot activate client");
		jack_ringbuffer_free (jd->ringbuffer[0]);
		jack_ringbuffer_free (jd->ringbuffer[1]);
		return -1;
	}

	jd->ports[0] = jack_port_register (jd->client, "left",
					   JACK_DEFAULT_AUDIO_TYPE,
					   JackPortIsOutput, 0);
	if ( !jd->ports[0] ) {
		ERROR ("Cannot register output port.\n");
		jack_ringbuffer_free (jd->ringbuffer[0]);
		jack_ringbuffer_free (jd->ringbuffer[1]);
		return -1;
	}

	jd->ports[1] = jack_port_register (jd->client, "right",
					   JACK_DEFAULT_AUDIO_TYPE,
					   JackPortIsOutput, 0);
	if ( !jd->ports[1] ) {
		ERROR ("Cannot register output port.\n");
		jack_ringbuffer_free (jd->ringbuffer[0]);
		jack_ringbuffer_free (jd->ringbuffer[1]);
		return -1;
	}

	memset (jd->ringbuffer[0]->buf, 0, jd->ringbuffer[0]->size);
	memset (jd->ringbuffer[1]->buf, 0, jd->ringbuffer[1]->size);

	/*  hay que buscar que hay  */
	if ( !ports[1] && (jports = jack_get_ports (jd->client, NULL, NULL,
						    JackPortIsPhysical|
						    JackPortIsInput)) ) {
		ports[0] = (char *) jports[0];
		ports[1] = (char *) ( jports[1] ? jports[1] : jports[0] );
		ERROR ("jports: %s %s\n", ports[0], ports[1]);
		free (jports);
	}

	if ( ports[1] )  {
		if ( (jack_connect (jd->client, "mpd:left", ports[0])) != 0 ) {
			ERROR ("%s is not a valid Jack Client / Port ", ports[0]);
			jack_ringbuffer_free (jd->ringbuffer[0]);
			jack_ringbuffer_free (jd->ringbuffer[1]);
			return -1;
		}
		if ( (jack_connect (jd->client, "mpd:right", ports[1])) != 0 ) {
			ERROR ("%s is not a valid Jack Client / Port ", ports[1]);
			jack_ringbuffer_free (jd->ringbuffer[0]);
			jack_ringbuffer_free (jd->ringbuffer[1]);
			return -1;
		}
	}

	ERROR ("connect_jack (pid=%d)\n", getpid ());
	return 1;
}

static int jack_initDriver (AudioOutput *audioOutput, ConfigParam *param)
{
	BlockParam *bp;
	char *endptr;
	int val;
 	char *cp = NULL;

	if ( param ) {
		bp = getBlockParam (param, "ports");
 		if ( bp ) {
 			cp = strdup (bp->value);
 			ports[0] = strdup (strtok (cp, " ,"));
 			ports[1] = strdup (strtok (NULL, " ,"));
			free (cp);
		}

		bp = getBlockParam (param, "ringbuffer_size");
 		if ( bp ) {
			errno = 0;
 			val = strtol (bp->value, &endptr, 10);

			if ( errno == 0 && endptr != bp->value) {
				ringbuf_sz = val;
				ERROR ("ringbuffer_size=%d\n", ringbuf_sz);
			} else {
				ERROR ("%s is not a number; ringbuf_size=%d\n",
				       bp->value, ringbuf_sz);
			}
		}
	}

	ERROR ("jack_initDriver (pid=%d)\n", getpid ());

 	return 0;

}

static int jack_testDefault(void)
{
	return 0;
}

static int jack_openDevice (AudioOutput *audioOutput)
{
	if ( !jd ) {
 		jd = newJackData ();
		audioOutput->data = jd;

		if ( !connect_jack (audioOutput) ) {
 			free (jd);
			return -1;
 		}
	}

	set_audioformat (audioOutput);
	audioOutput->open = 1;

	ERROR ("jack_openDevice (pid=%d)!\n", getpid ());
	return 0;
}


static void jack_closeDevice(AudioOutput * audioOutput)
{
	audioOutput->open = 0;
	ERROR ("jack_closeDevice (pid=%d)!\n", getpid ());
}

static void jack_dropBufferedAudio (AudioOutput * audioOutput)
{

}

static int jack_playAudio(AudioOutput * audioOutput, char *buff, int size)
{
	JackData *jd = audioOutput->data;
	size_t remain = size;
	size_t pos = 0;

	if ( jd->shutdown ) {
		ERROR ("Refusing to play, because there is no client thread.\n");
		return 0;
	}

	if ( jd->our_xrun ) {
		ERROR ("xrun\n");
		jd->our_xrun = 0;
	}

	while (remain && !jd->shutdown) {
		size_t space;

		if ( (space = jack_ringbuffer_write_space (jd->ringbuffer[0]))
		     > sizeof (jack_default_audio_sample_t) ) {
			size_t to_write;

			to_write = MIN (space, remain);
			remain -= to_write;
			to_write /= 4;
			//ERROR ("\t\tto_write=%d    remain=%d (%d)\n", to_write, remain, to_write * 2 * 2);
			while (to_write--) {
 				jack_default_audio_sample_t sample;

				sample = *(short *)(buff + pos);
				sample /= 32768;
				pos += 2;
				jack_ringbuffer_write (jd->ringbuffer[0],
						       (char *)&sample,
						       sizeof (sample));


				sample = *(short *)(buff + pos);
				sample /= 32768;
				pos += 2;
				jack_ringbuffer_write (jd->ringbuffer[1],
						       (char *)&sample,
						       sizeof (sample));
			}
		} else {
			usleep (ringbuf_sz / (float)(jd->bps) * 100000.0);
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
