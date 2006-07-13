#include "../audioOutput.h"

#include <stdlib.h>

#ifdef HAVE_PULSE

#define MPD_PULSE_NAME "mpd"
#define MPD_PULSE_STREAM_NAME "mpd"

#include "../conf.h"
#include "../log.h"

#include <pulse/simple.h>
#include <pulse/error.h>

typedef struct _PulseData {
	char * server;
	char * sink;
	pa_simple * s;
} PulseData;

static PulseData * newPulseData()
{
	PulseData * ret;
	
	ret = malloc(sizeof(PulseData));
	ret->server = NULL;
	ret->sink = NULL;
	ret->s = NULL;
	return ret;
}

static void freePulseData(PulseData * ad)
{
	if (ad->server) free(ad->server);
	if (ad->sink) free(ad->sink);
	free(ad);
}

static int pulse_initDriver(AudioOutput * audioOutput, ConfigParam * param)
{
	BlockParam * server = NULL;
	BlockParam * sink = NULL;
	PulseData * ad;

	if (param) {
		server = getBlockParam(param, "server");
		sink = getBlockParam(param, "sink");
	}

	ad = newPulseData();
	ad->server = server ? strdup(server->value) : NULL;
	ad->sink = sink ? strdup(sink->value) : NULL;
	audioOutput->data = ad;

	return 0;
}

static void pulse_finishDriver(AudioOutput * audioOutput)
{
	freePulseData((PulseData *) audioOutput->data);
}

static int pulse_testDefault()
{
	pa_simple * s;
	pa_sample_spec ss;
	int error;

	ss.format = PA_SAMPLE_S16NE;
	ss.rate = 44100;
	ss.channels = 2;

	s = pa_simple_new(NULL, MPD_PULSE_NAME, PA_STREAM_PLAYBACK, NULL,
	                  MPD_PULSE_STREAM_NAME, &ss, NULL, NULL, &error);
	if (!s) {
		WARNING("Cannot connect to default PulseAudio server: %s\n",
		        pa_strerror(error));
		return -1;
	}

	pa_simple_free(s);

	return 0;
}

static int pulse_openDevice(AudioOutput * audioOutput)
{
	PulseData * ad;
	AudioFormat * audioFormat;
	pa_sample_spec ss;
	int error;

	ad = audioOutput->data;
	audioFormat = &audioOutput->outAudioFormat;

	if (audioFormat->bits != 16) {
		ERROR("PulseAudio doesn't support %i bit audio\n",
		      audioFormat->bits);
		return -1;
	}

	ss.format = PA_SAMPLE_S16NE;
	ss.rate = audioFormat->sampleRate;
	ss.channels = audioFormat->channels;

	ad->s = pa_simple_new(ad->server, MPD_PULSE_NAME, PA_STREAM_PLAYBACK,
	                       ad->sink, MPD_PULSE_STREAM_NAME, &ss,
	                       NULL, NULL, &error);
	if (!ad->s) {
		ERROR("Cannot connect to server in PulseAudio output " \
		      "\"%s\": %s\n", audioOutput->name, pa_strerror(error));
		return -1;
	}

	audioOutput->open = 1;

	DEBUG("PulseAudio output \"%s\" connected and playing %i bit, %i " \
	      "channel audio at %i Hz\n", audioOutput->name, audioFormat->bits,
	      audioFormat->channels, audioFormat->sampleRate);

	return 0;
}

static void pulse_dropBufferedAudio(AudioOutput * audioOutput)
{
	PulseData * ad;
	int error;

	ad = audioOutput->data;
	if (pa_simple_flush(ad->s, &error) < 0) 
		WARNING("Flush failed in PulseAudio output \"%s\": %s\n",
		        audioOutput->name, pa_strerror(error));
}

static void pulse_closeDevice(AudioOutput * audioOutput)
{
	PulseData * ad;

	ad = audioOutput->data;
	if (ad->s) {
		pa_simple_drain(ad->s, NULL);
		pa_simple_free(ad->s);
	}

	audioOutput->open = 0;
}

static int pulse_playAudio(AudioOutput * audioOutput, char * playChunk,
                           int size)
{
	PulseData * ad;
	int error;

	ad = audioOutput->data;

	if (pa_simple_write(ad->s, playChunk, size, &error) < 0) {
		ERROR("PulseAudio output \"%s\" disconnecting due to write " \
		      "error: %s\n", audioOutput->name, pa_strerror(error));
		pulse_closeDevice(audioOutput);
		return -1;
	}

	return 0;
}

AudioOutputPlugin pulsePlugin = {
	"pulse",
	pulse_testDefault,
	pulse_initDriver,
	pulse_finishDriver,
	pulse_openDevice,
	pulse_playAudio,
	pulse_dropBufferedAudio,
	pulse_closeDevice,
	NULL, /* sendMetadataFunc */
};

#else /* HAVE_PULSE */

AudioOutputPlugin pulsePlugin =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

#endif /* HAVE_PULSE */
