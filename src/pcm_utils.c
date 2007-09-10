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

#include "pcm_utils.h"

#include "mpd_types.h"
#include "log.h"
#include "utils.h"
#include "conf.h"

#include <string.h>
#include <math.h>
#include <assert.h>

void pcm_volumeChange(char *buffer, int bufferSize, AudioFormat * format,
                      int volume)
{
	mpd_sint32 temp32;
	mpd_sint8 *buffer8 = (mpd_sint8 *) buffer;
	mpd_sint16 *buffer16 = (mpd_sint16 *) buffer;

	if (volume >= 1000)
		return;

	if (volume <= 0) {
		memset(buffer, 0, bufferSize);
		return;
	}

	switch (format->bits) {
	case 16:
		while (bufferSize > 0) {
			temp32 = *buffer16;
			temp32 *= volume;
			temp32 += rand() & 511;
			temp32 -= rand() & 511;
			temp32 += 500;
			temp32 /= 1000;
			*buffer16 = temp32 > 32767 ? 32767 :
			    (temp32 < -32768 ? -32768 : temp32);
			buffer16++;
			bufferSize -= 2;
		}
		break;
	case 8:
		while (bufferSize > 0) {
			temp32 = *buffer8;
			temp32 *= volume;
			temp32 += rand() & 511;
			temp32 -= rand() & 511;
			temp32 += 500;
			temp32 /= 1000;
			*buffer8 = temp32 > 127 ? 127 :
			    (temp32 < -128 ? -128 : temp32);
			buffer8++;
			bufferSize--;
		}
		break;
	default:
		FATAL("%i bits not supported by pcm_volumeChange!\n",
		      format->bits);
	}
}

static void pcm_add(char *buffer1, char *buffer2, size_t bufferSize1,
                    size_t bufferSize2, int vol1, int vol2,
                    AudioFormat * format)
{
	mpd_sint32 temp32;
	mpd_sint8 *buffer8_1 = (mpd_sint8 *) buffer1;
	mpd_sint8 *buffer8_2 = (mpd_sint8 *) buffer2;
	mpd_sint16 *buffer16_1 = (mpd_sint16 *) buffer1;
	mpd_sint16 *buffer16_2 = (mpd_sint16 *) buffer2;

	switch (format->bits) {
	case 16:
		while (bufferSize1 > 0 && bufferSize2 > 0) {
			temp32 =
			    (vol1 * (*buffer16_1) +
			     vol2 * (*buffer16_2));
			temp32 += rand() & 511;
			temp32 -= rand() & 511;
			temp32 += 500;
			temp32 /= 1000;
			*buffer16_1 =
			    temp32 > 32767 ? 32767 : (temp32 <
						      -32768 ? -32768 : temp32);
			buffer16_1++;
			buffer16_2++;
			bufferSize1 -= 2;
			bufferSize2 -= 2;
		}
		if (bufferSize2 > 0)
			memcpy(buffer16_1, buffer16_2, bufferSize2);
		break;
	case 8:
		while (bufferSize1 > 0 && bufferSize2 > 0) {
			temp32 =
			    (vol1 * (*buffer8_1) + vol2 * (*buffer8_2));
			temp32 += rand() & 511;
			temp32 -= rand() & 511;
			temp32 += 500;
			temp32 /= 1000;
			*buffer8_1 =
			    temp32 > 127 ? 127 : (temp32 <
						  -128 ? -128 : temp32);
			buffer8_1++;
			buffer8_2++;
			bufferSize1--;
			bufferSize2--;
		}
		if (bufferSize2 > 0)
			memcpy(buffer8_1, buffer8_2, bufferSize2);
		break;
	default:
		FATAL("%i bits not supported by pcm_add!\n", format->bits);
	}
}

void pcm_mix(char *buffer1, char *buffer2, size_t bufferSize1,
             size_t bufferSize2, AudioFormat * format, float portion1)
{
	int vol1;
	float s = sin(M_PI_2 * portion1);
	s *= s;

	vol1 = s * 1000 + 0.5;
	vol1 = vol1 > 1000 ? 1000 : (vol1 < 0 ? 0 : vol1);

	pcm_add(buffer1, buffer2, bufferSize1, bufferSize2, vol1, 1000 - vol1,
		format);
}

#ifdef HAVE_LIBSAMPLERATE
static int pcm_getSampleRateConverter(void)
{
	const char *conf = getConfigParamValue(CONF_SAMPLERATE_CONVERTER);
	long convalgo;
	char *test;
	size_t len;

	if (!conf) {
		convalgo = SRC_SINC_FASTEST;
		goto out;
	}

	convalgo = strtol(conf, &test, 10);
	if (*test == '\0' && src_get_name(convalgo))
		goto out;

	len = strlen(conf);
	for (convalgo = 0 ; ; convalgo++) {
		test = (char *)src_get_name(convalgo);
		if (!test) {
			convalgo = SRC_SINC_FASTEST;
			break;
		}
		if (strncasecmp(test, conf, len) == 0)
			goto out;
	}

	ERROR("unknown samplerate converter \"%s\"\n", conf);
out:
	DEBUG("selecting samplerate converter \"%s\"\n",
	      src_get_name(convalgo));

	return convalgo;
}
#endif

#ifdef HAVE_LIBSAMPLERATE
static size_t pcm_convertSampleRate(mpd_sint8 channels, mpd_uint32 inSampleRate,
                                    char *inBuffer, size_t inSize,
                                    mpd_uint32 outSampleRate, char *outBuffer,
                                    size_t outSize, ConvState *convState)
{
	static int convalgo = -1;
	SRC_DATA *data = &convState->data;
	size_t dataInSize;
	size_t dataOutSize;
	int error;

	if (convalgo < 0)
		convalgo = pcm_getSampleRateConverter();

	/* (re)set the state/ratio if the in or out format changed */
	if ((channels != convState->lastChannels) ||
	    (inSampleRate != convState->lastInSampleRate) ||
	    (outSampleRate != convState->lastOutSampleRate)) {
		convState->error = 0;
		convState->lastChannels = channels;
		convState->lastInSampleRate = inSampleRate;
		convState->lastOutSampleRate = outSampleRate;

		if (convState->state)
			convState->state = src_delete(convState->state);

		convState->state = src_new(convalgo, channels, &error);
		if (!convState->state) {
			ERROR("cannot create new libsamplerate state: %s\n",
			      src_strerror(error));
			convState->error = 1;
			return 0;
		}
		
		data->src_ratio = (double)outSampleRate / (double)inSampleRate;
		DEBUG("setting samplerate conversion ratio to %.2lf\n",
		      data->src_ratio);
		src_set_ratio(convState->state, data->src_ratio);
	}

	/* there was an error previously, and nothing has changed */
	if (convState->error)
		return 0;

	data->input_frames = inSize / 2 / channels;
	dataInSize = data->input_frames * sizeof(float) * channels;
	if (dataInSize > convState->dataInSize) {
		convState->dataInSize = dataInSize;
		data->data_in = xrealloc(data->data_in, dataInSize);
	}

	data->output_frames = outSize / 2 / channels;
	dataOutSize = data->output_frames * sizeof(float) * channels;
	if (dataOutSize > convState->dataOutSize) {
		convState->dataOutSize = dataOutSize;
		data->data_out = xrealloc(data->data_out, dataOutSize);
	}

	src_short_to_float_array((short *)inBuffer, data->data_in,
	                         data->input_frames * channels);

	error = src_process(convState->state, data);
	if (error) {
		ERROR("error processing samples with libsamplerate: %s\n",
		      src_strerror(error));
		convState->error = 1;
		return 0;
	}

	src_float_to_short_array(data->data_out, (short *)outBuffer,
	                         data->output_frames_gen * channels);

	return data->output_frames_gen * 2 * channels;
}
#else /* !HAVE_LIBSAMPLERATE */
/* resampling code blatantly ripped from ESD */
static size_t pcm_convertSampleRate(mpd_sint8 channels, mpd_uint32 inSampleRate,
                                    char *inBuffer, size_t inSize,
                                    mpd_uint32 outSampleRate, char *outBuffer,
                                    size_t outSize, ConvState *convState)
{
	mpd_uint32 rd_dat = 0;
	mpd_uint32 wr_dat = 0;
	mpd_sint16 *in = (mpd_sint16 *)inBuffer;
	mpd_sint16 *out = (mpd_sint16 *)outBuffer;
	mpd_uint32 nlen = outSize / 2;
	mpd_sint16 lsample, rsample;

	switch (channels) {
	case 1:
		while (wr_dat < nlen) {
			rd_dat = wr_dat * inSampleRate / outSampleRate;

			lsample = in[rd_dat++];

			out[wr_dat++] = lsample;
		}
		break;
	case 2:
		while (wr_dat < nlen) {
			rd_dat = wr_dat * inSampleRate / outSampleRate;
			rd_dat &= ~1;

			lsample = in[rd_dat++];
			rsample = in[rd_dat++];

			out[wr_dat++] = lsample;
			out[wr_dat++] = rsample;
		}
		break;
	}

	return outSize;
}
#endif /* !HAVE_LIBSAMPLERATE */

static char *pcm_convertChannels(mpd_sint8 channels, char *inBuffer,
                                 size_t inSize, size_t *outSize)
{
	static char *buf;
	static size_t len;
	char *outBuffer = NULL;
	mpd_sint16 *in;
	mpd_sint16 *out;
	int inSamples, i;

	switch (channels) {
	/* convert from 1 -> 2 channels */
	case 1:
		*outSize = (inSize >> 1) << 2;
		if (*outSize > len) {
			len = *outSize;
			buf = xrealloc(buf, len);
		}
		outBuffer = buf;

		inSamples = inSize >> 1;
		in = (mpd_sint16 *)inBuffer;
		out = (mpd_sint16 *)outBuffer;
		for (i = 0; i < inSamples; i++) {
			*out++ = *in;
			*out++ = *in++;
		}

		break;
	/* convert from 2 -> 1 channels */
	case 2:
		*outSize = inSize >> 1;
		if (*outSize > len) {
			len = *outSize;
			buf = xrealloc(buf, len);
		}
		outBuffer = buf;

		inSamples = inSize >> 2;
		in = (mpd_sint16 *)inBuffer;
		out = (mpd_sint16 *)outBuffer;
		for (i = 0; i < inSamples; i++) {
			*out = (*in++) / 2;
			*out++ += (*in++) / 2;
		}

		break;
	default:
		ERROR("only 1 or 2 channels are supported for conversion!\n");
	}

	return outBuffer;
}

static char *pcm_convertTo16bit(mpd_sint8 bits, char *inBuffer, size_t inSize,
                                size_t *outSize)
{
	static char *buf;
	static size_t len;
	char *outBuffer = NULL;
	mpd_sint8 *in;
	mpd_sint16 *out;
	int i;

	switch (bits) {
	case 8:
		*outSize = inSize << 1;
		if (*outSize > len) {
			len = *outSize;
			buf = xrealloc(buf, len);
		}
		outBuffer = buf;

		in = (mpd_sint8 *)inBuffer;
		out = (mpd_sint16 *)outBuffer;
		for (i = 0; i < inSize; i++)
			*out++ = (*in++) << 8;

		break;
	case 16:
		*outSize = inSize;
		outBuffer = inBuffer;
		break;
	case 24:
		/* put dithering code from mp3_decode here */
	default:
		ERROR("only 8 or 16 bits are supported for conversion!\n");
	}

	return outBuffer;
}

/* outFormat bits must be 16 and channels must be 1 or 2! */
size_t pcm_convertAudioFormat(AudioFormat * inFormat, char *inBuffer,
                              size_t inSize, AudioFormat * outFormat,
                              char *outBuffer, ConvState *convState)
{
	char *buf;
	size_t len;
	size_t outSize = pcm_sizeOfConvBuffer(inFormat, inSize, outFormat);

	assert(outFormat->bits == 16);
	assert(outFormat->channels == 2 || outFormat->channels == 1);

	/* everything else supports 16 bit only, so convert to that first */
	buf = pcm_convertTo16bit(inFormat->bits, inBuffer, inSize, &len);
	if (!buf)
		exit(EXIT_FAILURE);

	if (inFormat->channels != outFormat->channels) {
		buf = pcm_convertChannels(inFormat->channels, buf, len, &len);
		if (!buf)
			exit(EXIT_FAILURE);
	}

	if (inFormat->sampleRate == outFormat->sampleRate) {
		assert(outSize >= len);
		memcpy(outBuffer, buf, len);
	} else {
		len = pcm_convertSampleRate(outFormat->channels,
		                            inFormat->sampleRate, buf, len,
		                            outFormat->sampleRate, outBuffer,
		                            outSize, convState);
		if (len == 0)
			exit(EXIT_FAILURE);
	}

	return len;
}

size_t pcm_sizeOfConvBuffer(AudioFormat * inFormat, size_t inSize,
                            AudioFormat * outFormat)
{
	const double ratio = (double)outFormat->sampleRate /
	                     (double)inFormat->sampleRate;
	const int shift = 2 * outFormat->channels;
	size_t outSize = inSize;

	switch (inFormat->bits) {
	case 8:
		outSize <<= 1;
		break;
	case 16:
		break;
	default:
		FATAL("only 8 or 16 bits are supported for conversion!\n");
	}

	if (inFormat->channels != outFormat->channels) {
		switch (inFormat->channels) {
		case 1:
			outSize = (outSize >> 1) << 2;
			break;
		case 2:
			outSize >>= 1;
			break;
		default:
			FATAL("only 1 or 2 channels are supported "
			      "for conversion!\n");
		}
	}

	outSize /= shift;
	outSize = floor(0.5 + (double)outSize * ratio);
	outSize *= shift;

	return outSize;
}
