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
#include "log.h"
#include "utils.h"
#include "conf.h"
#include "audio_format.h"

#include <assert.h>
#include <string.h>
#include <math.h>

static inline int
pcm_dither(void)
{
	return (rand() & 511) - (rand() & 511);
}

/**
 * Check if the value is within the range of the provided bit size,
 * and caps it if necessary.
 */
static int32_t
pcm_range(int32_t sample, unsigned bits)
{
	if (mpd_unlikely(sample < (-1 << (bits - 1))))
		return -1 << (bits - 1);
	if (mpd_unlikely(sample >= (1 << (bits - 1))))
		return (1 << (bits - 1)) - 1;
	return sample;
}

static void
pcm_volume_change_8(int8_t *buffer, unsigned num_samples, int volume)
{
	while (num_samples > 0) {
		int32_t sample = *buffer;

		sample = (sample * volume + pcm_dither() + 500) / 1000;

		*buffer++ = pcm_range(sample, 8);
		--num_samples;
	}
}

static void
pcm_volume_change_16(int16_t *buffer, unsigned num_samples, int volume)
{
	while (num_samples > 0) {
		int32_t sample = *buffer;

		sample = (sample * volume + pcm_dither() + 500) / 1000;

		*buffer++ = pcm_range(sample, 16);
		--num_samples;
	}
}

static void
pcm_volume_change_24(int32_t *buffer, unsigned num_samples, int volume)
{
	while (num_samples > 0) {
		int64_t sample = *buffer;

		sample = (sample * volume + pcm_dither() + 500) / 1000;

		*buffer++ = pcm_range(sample, 24);
		--num_samples;
	}
}

void pcm_volumeChange(char *buffer, int bufferSize,
                      const struct audio_format *format,
                      int volume)
{
	if (volume >= 1000)
		return;

	if (volume <= 0) {
		memset(buffer, 0, bufferSize);
		return;
	}

	switch (format->bits) {
	case 8:
		pcm_volume_change_8((int8_t *)buffer, bufferSize, volume);
		break;

	case 16:
		pcm_volume_change_16((int16_t *)buffer, bufferSize / 2,
				     volume);
		break;

	case 24:
		pcm_volume_change_24((int32_t*)buffer, bufferSize / 4,
				     volume);
		break;

	default:
		FATAL("%u bits not supported by pcm_volumeChange!\n",
		      format->bits);
	}
}

static void
pcm_add_8(int8_t *buffer1, const int8_t *buffer2,
	  unsigned num_samples, int volume1, int volume2)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_dither() + 500) / 1000;

		*buffer1++ = pcm_range(sample1, 8);
		--num_samples;
	}
}

static void
pcm_add_16(int16_t *buffer1, const int16_t *buffer2,
	   unsigned num_samples, int volume1, int volume2)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_dither() + 500) / 1000;

		*buffer1++ = pcm_range(sample1, 16);
		--num_samples;
	}
}

static void
pcm_add_24(int32_t *buffer1, const int32_t *buffer2,
	   unsigned num_samples, unsigned volume1, unsigned volume2)
{
	while (num_samples > 0) {
		int64_t sample1 = *buffer1;
		int64_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_dither() + 500) / 1000;

		*buffer1++ = pcm_range(sample1, 24);
		--num_samples;
	}
}

static void pcm_add(char *buffer1, const char *buffer2, size_t size,
                    int vol1, int vol2,
                    const struct audio_format *format)
{
	switch (format->bits) {
	case 8:
		pcm_add_8((int8_t *)buffer1, (const int8_t *)buffer2,
			  size, vol1, vol2);
		break;

	case 16:
		pcm_add_16((int16_t *)buffer1, (const int16_t *)buffer2,
			   size / 2, vol1, vol2);
		break;

	case 24:
		pcm_add_24((int32_t*)buffer1,
			   (const int32_t*)buffer2,
			   size / 4, vol1, vol2);
		break;

	default:
		FATAL("%u bits not supported by pcm_add!\n", format->bits);
	}
}

void pcm_mix(char *buffer1, const char *buffer2, size_t size,
             const struct audio_format *format, float portion1)
{
	int vol1;
	float s = sin(M_PI_2 * portion1);
	s *= s;

	vol1 = s * 1000 + 0.5;
	vol1 = vol1 > 1000 ? 1000 : (vol1 < 0 ? 0 : vol1);

	pcm_add(buffer1, buffer2, size, vol1, 1000 - vol1, format);
}

#ifdef HAVE_LIBSAMPLERATE
static int pcm_getSampleRateConverter(void)
{
	const char *conf = getConfigParamValue(CONF_SAMPLERATE_CONVERTER);
	long convalgo;
	char *test;
	const char *test2;
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
		test2 = src_get_name(convalgo);
		if (!test2) {
			convalgo = SRC_SINC_FASTEST;
			break;
		}
		if (strncasecmp(test2, conf, len) == 0)
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
static size_t pcm_convertSampleRate(int8_t channels, uint32_t inSampleRate,
                                    const int16_t *inBuffer, size_t inSize,
                                    uint32_t outSampleRate, int16_t *outBuffer,
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

	src_short_to_float_array((const short *)inBuffer, data->data_in,
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
static size_t pcm_convertSampleRate(int8_t channels, uint32_t inSampleRate,
                                    const int16_t *inBuffer,
                                    mpd_unused size_t inSize,
                                    uint32_t outSampleRate, char *outBuffer,
                                    size_t outSize,
                                    mpd_unused ConvState *convState)
{
	uint32_t rd_dat = 0;
	uint32_t wr_dat = 0;
	const int16_t *in = (const int16_t *)inBuffer;
	int16_t *out = (int16_t *)outBuffer;
	uint32_t nlen = outSize / 2;
	int16_t lsample, rsample;

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

static void
pcm_convert_channels_1_to_2(int16_t *dest, const int16_t *src,
			    unsigned num_frames)
{
	while (num_frames-- > 0) {
		int16_t value = *src++;

		*dest++ = value;
		*dest++ = value;
	}
}

static void
pcm_convert_channels_2_to_1(int16_t *dest, const int16_t *src,
			    unsigned num_frames)
{
	while (num_frames-- > 0) {
		int32_t a = *src++, b = *src++;

		*dest++ = (a + b) / 2;
	}
}

static const int16_t *
pcm_convertChannels(int8_t dest_channels,
		    int8_t src_channels, const int16_t *src,
		    size_t src_size, size_t *dest_size_r)
{
	static int16_t *buf;
	static size_t len;
	unsigned num_frames = src_size / src_channels / sizeof(*src);
	unsigned dest_size = num_frames * dest_channels * sizeof(*src);

	if (dest_size > len) {
		len = dest_size;
		buf = xrealloc(buf, len);
	}

	*dest_size_r = dest_size;

	if (src_channels == 1 && dest_channels == 2)
		pcm_convert_channels_1_to_2(buf, src, num_frames);
	else if (src_channels == 2 && dest_channels == 1)
		pcm_convert_channels_2_to_1(buf, src, num_frames);
	else {
		ERROR("conversion %u->%u channels is not supported\n",
		      src_channels, dest_channels);
		return NULL;
	}

	return buf;
}

static void
pcm_convert_8_to_16(int16_t *out, const int8_t *in,
		    unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ << 8;
		--num_samples;
	}
}

static void
pcm_convert_24_to_16(int16_t *out, const int32_t *in,
		     unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ >> 8;
		--num_samples;
	}
}

static const int16_t *
pcm_convertTo16bit(uint8_t bits, const void *inBuffer,
		   size_t inSize, size_t *outSize)
{
	static int16_t *buf;
	static size_t len;
	unsigned num_samples;

	switch (bits) {
	case 8:
		num_samples = inSize;
		*outSize = inSize << 1;
		if (*outSize > len) {
			len = *outSize;
			buf = xrealloc(buf, len);
		}

		pcm_convert_8_to_16((int16_t *)buf,
				    (const int8_t *)inBuffer,
				    num_samples);
		return buf;

	case 16:
		*outSize = inSize;
		return inBuffer;

	case 24:
		num_samples = inSize / 4;
		*outSize = num_samples * 2;
		if (*outSize > len) {
			len = *outSize;
			buf = xrealloc(buf, len);
		}

		pcm_convert_24_to_16((int16_t *)buf,
				     (const int32_t *)inBuffer,
				     num_samples);
		return buf;
	}

	ERROR("only 8 or 16 bits are supported for conversion!\n");
	return NULL;
}

/* outFormat bits must be 16 and channels must be 1 or 2! */
size_t pcm_convertAudioFormat(const struct audio_format *inFormat,
			      const char *inBuffer, size_t inSize,
			      const struct audio_format *outFormat,
                              char *outBuffer, ConvState *convState)
{
	const int16_t *buf;
	size_t len = 0;
	size_t outSize = pcm_sizeOfConvBuffer(inFormat, inSize, outFormat);

	assert(outFormat->bits == 16);

	/* everything else supports 16 bit only, so convert to that first */
	buf = pcm_convertTo16bit(inFormat->bits, inBuffer, inSize, &len);
	if (!buf)
		exit(EXIT_FAILURE);

	if (inFormat->channels != outFormat->channels) {
		buf = pcm_convertChannels(outFormat->channels,
					  inFormat->channels,
					  buf, len, &len);
		if (!buf)
			exit(EXIT_FAILURE);
	}

	if (inFormat->sample_rate == outFormat->sample_rate) {
		assert(outSize >= len);
		memcpy(outBuffer, buf, len);
	} else {
		len = pcm_convertSampleRate(outFormat->channels,
		                            inFormat->sample_rate, buf, len,
		                            outFormat->sample_rate,
		                            (int16_t*)outBuffer,
		                            outSize, convState);
		if (len == 0)
			exit(EXIT_FAILURE);
	}

	return len;
}

size_t pcm_sizeOfConvBuffer(const struct audio_format *inFormat, size_t inSize,
                            const struct audio_format *outFormat)
{
	const double ratio = (double)outFormat->sample_rate /
	                     (double)inFormat->sample_rate;
	const int shift = 2 * outFormat->channels;
	size_t outSize = inSize;

	switch (inFormat->bits) {
	case 8:
		outSize <<= 1;
		break;
	case 16:
		break;
	case 24:
		outSize = (outSize / 4) * 2;
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
