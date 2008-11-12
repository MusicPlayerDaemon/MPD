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

#include "../decoder_api.h"

#define AAC_MAX_CHANNELS	6

#include "../utils.h"
#include "../log.h"

#include <assert.h>
#include <faad.h>

/* all code here is either based on or copied from FAAD2's frontend code */
typedef struct {
	struct decoder *decoder;
	struct input_stream *inStream;
	size_t bytesIntoBuffer;
	size_t bytesConsumed;
	off_t fileOffset;
	unsigned char *buffer;
} AacBuffer;

static void aac_buffer_shift(AacBuffer * b, size_t length)
{
	assert(length >= b->bytesConsumed);
	assert(length <= b->bytesConsumed + b->bytesIntoBuffer);

	memmove(b->buffer, b->buffer + length,
		b->bytesConsumed + b->bytesIntoBuffer - length);

	length -= b->bytesConsumed;
	b->bytesConsumed = 0;
	b->bytesIntoBuffer -= length;
}

static void fillAacBuffer(AacBuffer * b)
{
	size_t rest, bread;

	if (b->bytesIntoBuffer >= FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS)
		/* buffer already full */
		return;

	aac_buffer_shift(b, b->bytesConsumed);

	rest = FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS - b->bytesIntoBuffer;
	bread = decoder_read(b->decoder, b->inStream,
			     (void *)(b->buffer + b->bytesIntoBuffer),
			     rest);
	b->bytesIntoBuffer += bread;

	if ((b->bytesIntoBuffer > 3 && memcmp(b->buffer, "TAG", 3) == 0) ||
	    (b->bytesIntoBuffer > 11 &&
	     memcmp(b->buffer, "LYRICSBEGIN", 11) == 0) ||
	    (b->bytesIntoBuffer > 8 && memcmp(b->buffer, "APETAGEX", 8) == 0))
		b->bytesIntoBuffer = 0;
}

static void advanceAacBuffer(AacBuffer * b, size_t bytes)
{
	b->fileOffset += bytes;
	b->bytesConsumed = bytes;
	b->bytesIntoBuffer -= bytes;
}

static const unsigned adtsSampleRates[] =
    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

/**
 * Check whether the buffer head is an AAC frame, and return the frame
 * length.  Returns 0 if it is not a frame.
 */
static size_t adts_check_frame(AacBuffer * b)
{
	if (b->bytesIntoBuffer <= 7)
		return 0;

	/* check syncword */
	if (!((b->buffer[0] == 0xFF) && ((b->buffer[1] & 0xF6) == 0xF0)))
		return 0;

	return (((unsigned int)b->buffer[3] & 0x3) << 11) |
		(((unsigned int)b->buffer[4]) << 3) |
		(b->buffer[5] >> 5);
}

/**
 * Find the next AAC frame in the buffer.  Returns 0 if no frame is
 * found or if not enough data is available.
 */
static size_t adts_find_frame(AacBuffer * b)
{
	const unsigned char *p;
	size_t frame_length;

	while ((p = memchr(b->buffer, 0xff, b->bytesIntoBuffer)) != NULL) {
		/* discard data before 0xff */
		if (p > b->buffer)
			aac_buffer_shift(b, p - b->buffer);

		if (b->bytesIntoBuffer <= 7)
			/* not enough data yet */
			return 0;

		/* is it a frame? */
		frame_length = adts_check_frame(b);
		if (frame_length > 0)
			/* yes, it is */
			return frame_length;

		/* it's just some random 0xff byte; discard and and
		   continue searching */
		aac_buffer_shift(b, 1);
	}

	/* nothing at all; discard the whole buffer */
	aac_buffer_shift(b, b->bytesIntoBuffer);
	return 0;
}

static void adtsParse(AacBuffer * b, float *length)
{
	unsigned int frames, frameLength;
	unsigned sample_rate = 0;
	float framesPerSec;

	/* Read all frames to ensure correct time and bitrate */
	for (frames = 0;; frames++) {
		fillAacBuffer(b);

		frameLength = adts_find_frame(b);
		if (frameLength > 0) {
			if (frames == 0) {
				sample_rate = adtsSampleRates[(b->
							       buffer[2] & 0x3c)
							      >> 2];
			}

			if (frameLength > b->bytesIntoBuffer)
				break;

			advanceAacBuffer(b, frameLength);
		} else
			break;
	}

	framesPerSec = (float)sample_rate / 1024.0;
	if (framesPerSec > 0)
		*length = (float)frames / framesPerSec;
}

static void
initAacBuffer(AacBuffer * b, struct decoder *decoder,
	      struct input_stream *inStream)
{
	memset(b, 0, sizeof(AacBuffer));

	b->decoder = decoder;
	b->inStream = inStream;

	b->buffer = xmalloc(FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS);
	memset(b->buffer, 0, FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS);
}

static void aac_parse_header(AacBuffer * b, float *length)
{
	size_t fileread;
	size_t tagsize;

	if (length)
		*length = -1;

	fileread = b->inStream->size;

	fillAacBuffer(b);

	tagsize = 0;
	if (b->bytesIntoBuffer >= 10 && !memcmp(b->buffer, "ID3", 3)) {
		tagsize = (b->buffer[6] << 21) | (b->buffer[7] << 14) |
		    (b->buffer[8] << 7) | (b->buffer[9] << 0);

		tagsize += 10;
		advanceAacBuffer(b, tagsize);
		fillAacBuffer(b);
	}

	if (length == NULL)
		return;

	if (b->inStream->seekable &&
	    b->bytesIntoBuffer >= 2 &&
	    (b->buffer[0] == 0xFF) && ((b->buffer[1] & 0xF6) == 0xF0)) {
		adtsParse(b, length);
		input_stream_seek(b->inStream, tagsize, SEEK_SET);

		b->bytesIntoBuffer = 0;
		b->bytesConsumed = 0;
		b->fileOffset = tagsize;

		fillAacBuffer(b);
	} else if (memcmp(b->buffer, "ADIF", 4) == 0) {
		unsigned bitRate;
		size_t skipSize = (b->buffer[4] & 0x80) ? 9 : 0;


		if (8 + skipSize > b->bytesIntoBuffer)
			/* not enough data yet; skip parsing this
			   header */
			return;

		bitRate =
		    ((unsigned int)(b->
				    buffer[4 +
					   skipSize] & 0x0F) << 19) | ((unsigned
									int)b->
								       buffer[5
									      +
									      skipSize]
								       << 11) |
		    ((unsigned int)b->
		     buffer[6 + skipSize] << 3) | ((unsigned int)b->buffer[7 +
									   skipSize]
						   & 0xE0);

		if (fileread != 0 && bitRate != 0)
			*length = fileread * 8.0 / bitRate;
		else
			*length = fileread;
	}
}

static float getAacFloatTotalTime(const char *file)
{
	AacBuffer b;
	float length;
	faacDecHandle decoder;
	faacDecConfigurationPtr config;
	uint32_t sample_rate;
	unsigned char channels;
	struct input_stream inStream;
	long bread;

	if (!input_stream_open(&inStream, file))
		return -1;

	initAacBuffer(&b, NULL, &inStream);
	aac_parse_header(&b, &length);

	if (length < 0) {
		decoder = faacDecOpen();

		config = faacDecGetCurrentConfiguration(decoder);
		config->outputFormat = FAAD_FMT_16BIT;
		faacDecSetConfiguration(decoder, config);

		fillAacBuffer(&b);
#ifdef HAVE_FAAD_BUFLEN_FUNCS
		bread = faacDecInit(decoder, b.buffer, b.bytesIntoBuffer,
				    &sample_rate, &channels);
#else
		bread = faacDecInit(decoder, b.buffer, &sample_rate, &channels);
#endif
		if (bread >= 0 && sample_rate > 0 && channels > 0)
			length = 0;

		faacDecClose(decoder);
	}

	if (b.buffer)
		free(b.buffer);
	input_stream_close(&inStream);

	return length;
}

static int getAacTotalTime(const char *file)
{
	int file_time = -1;
	float length;

	if ((length = getAacFloatTotalTime(file)) >= 0)
		file_time = length + 0.5;

	return file_time;
}

static void
aac_stream_decode(struct decoder *mpd_decoder, struct input_stream *inStream)
{
	float file_time;
	float totalTime = 0;
	faacDecHandle decoder;
	faacDecFrameInfo frameInfo;
	faacDecConfigurationPtr config;
	long bread;
	struct audio_format audio_format;
	uint32_t sample_rate;
	unsigned char channels;
	unsigned int sampleCount;
	char *sampleBuffer;
	size_t sampleBufferLen;
	uint16_t bitRate = 0;
	AacBuffer b;
	bool initialized = false;

	initAacBuffer(&b, mpd_decoder, inStream);

	decoder = faacDecOpen();

	config = faacDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
#ifdef HAVE_FAACDECCONFIGURATION_DOWNMATRIX
	config->downMatrix = 1;
#endif
#ifdef HAVE_FAACDECCONFIGURATION_DONTUPSAMPLEIMPLICITSBR
	config->dontUpSampleImplicitSBR = 0;
#endif
	faacDecSetConfiguration(decoder, config);

	while (b.bytesIntoBuffer < FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS &&
	       !input_stream_eof(b.inStream) &&
	       decoder_get_command(mpd_decoder) == DECODE_COMMAND_NONE) {
	       	fillAacBuffer(&b);
		adts_find_frame(&b);
		fillAacBuffer(&b);
		my_usleep(10000);
	}

#ifdef HAVE_FAAD_BUFLEN_FUNCS
	bread = faacDecInit(decoder, b.buffer, b.bytesIntoBuffer,
			    &sample_rate, &channels);
#else
	bread = faacDecInit(decoder, b.buffer, &sample_rate, &channels);
#endif
	if (bread < 0) {
		ERROR("Error not a AAC stream.\n");
		faacDecClose(decoder);
		if (b.buffer)
			free(b.buffer);
		return;
	}

	audio_format.bits = 16;

	file_time = 0.0;

	advanceAacBuffer(&b, bread);

	do {
		fillAacBuffer(&b);
		adts_find_frame(&b);
		fillAacBuffer(&b);

		if (b.bytesIntoBuffer == 0)
			break;

#ifdef HAVE_FAAD_BUFLEN_FUNCS
		sampleBuffer = faacDecDecode(decoder, &frameInfo, b.buffer,
					     b.bytesIntoBuffer);
#else
		sampleBuffer = faacDecDecode(decoder, &frameInfo, b.buffer);
#endif

		if (frameInfo.error > 0) {
			ERROR("error decoding AAC stream\n");
			ERROR("faad2 error: %s\n",
			      faacDecGetErrorMessage(frameInfo.error));
			break;
		}
#ifdef HAVE_FAACDECFRAMEINFO_SAMPLERATE
		sample_rate = frameInfo.samplerate;
#endif

		if (!initialized) {
			audio_format.channels = frameInfo.channels;
			audio_format.sample_rate = sample_rate;
			decoder_initialized(mpd_decoder, &audio_format,
					    false, totalTime);
			initialized = true;
		}

		advanceAacBuffer(&b, frameInfo.bytesconsumed);

		sampleCount = (unsigned long)(frameInfo.samples);

		if (sampleCount > 0) {
			bitRate = frameInfo.bytesconsumed * 8.0 *
			    frameInfo.channels * sample_rate /
			    frameInfo.samples / 1000 + 0.5;
			file_time +=
			    (float)(frameInfo.samples) / frameInfo.channels /
			    sample_rate;
		}

		sampleBufferLen = sampleCount * 2;

		decoder_data(mpd_decoder, NULL, sampleBuffer,
			     sampleBufferLen, file_time,
			     bitRate, NULL);
	} while (decoder_get_command(mpd_decoder) == DECODE_COMMAND_NONE);

	faacDecClose(decoder);
	if (b.buffer)
		free(b.buffer);
}


static void
aac_decode(struct decoder *mpd_decoder, const char *path)
{
	float file_time;
	float totalTime;
	faacDecHandle decoder;
	faacDecFrameInfo frameInfo;
	faacDecConfigurationPtr config;
	long bread;
	struct audio_format audio_format;
	uint32_t sample_rate;
	unsigned char channels;
	unsigned int sampleCount;
	char *sampleBuffer;
	size_t sampleBufferLen;
	/*float * seekTable;
	   long seekTableEnd = -1;
	   int seekPositionFound = 0; */
	uint16_t bitRate = 0;
	AacBuffer b;
	struct input_stream inStream;
	bool initialized = false;

	if ((totalTime = getAacFloatTotalTime(path)) < 0)
		return;

	if (!input_stream_open(&inStream, path))
		return;

	initAacBuffer(&b, mpd_decoder, &inStream);
	aac_parse_header(&b, NULL);

	decoder = faacDecOpen();

	config = faacDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
#ifdef HAVE_FAACDECCONFIGURATION_DOWNMATRIX
	config->downMatrix = 1;
#endif
#ifdef HAVE_FAACDECCONFIGURATION_DONTUPSAMPLEIMPLICITSBR
	config->dontUpSampleImplicitSBR = 0;
#endif
	faacDecSetConfiguration(decoder, config);

	fillAacBuffer(&b);

#ifdef HAVE_FAAD_BUFLEN_FUNCS
	bread = faacDecInit(decoder, b.buffer, b.bytesIntoBuffer,
			    &sample_rate, &channels);
#else
	bread = faacDecInit(decoder, b.buffer, &sample_rate, &channels);
#endif
	if (bread < 0) {
		ERROR("Error not a AAC stream.\n");
		faacDecClose(decoder);
		if (b.buffer)
			free(b.buffer);
		return;
	}

	audio_format.bits = 16;

	file_time = 0.0;

	advanceAacBuffer(&b, bread);

	do {
		fillAacBuffer(&b);

		if (b.bytesIntoBuffer == 0)
			break;

#ifdef HAVE_FAAD_BUFLEN_FUNCS
		sampleBuffer = faacDecDecode(decoder, &frameInfo, b.buffer,
					     b.bytesIntoBuffer);
#else
		sampleBuffer = faacDecDecode(decoder, &frameInfo, b.buffer);
#endif

		if (frameInfo.error > 0) {
			ERROR("error decoding AAC file: %s\n", path);
			ERROR("faad2 error: %s\n",
			      faacDecGetErrorMessage(frameInfo.error));
			break;
		}
#ifdef HAVE_FAACDECFRAMEINFO_SAMPLERATE
		sample_rate = frameInfo.samplerate;
#endif

		if (!initialized) {
			audio_format.channels = frameInfo.channels;
			audio_format.sample_rate = sample_rate;
			decoder_initialized(mpd_decoder, &audio_format,
					    false, totalTime);
			initialized = true;
		}

		advanceAacBuffer(&b, frameInfo.bytesconsumed);

		sampleCount = (unsigned long)(frameInfo.samples);

		if (sampleCount > 0) {
			bitRate = frameInfo.bytesconsumed * 8.0 *
			    frameInfo.channels * sample_rate /
			    frameInfo.samples / 1000 + 0.5;
			file_time +=
			    (float)(frameInfo.samples) / frameInfo.channels /
			    sample_rate;
		}

		sampleBufferLen = sampleCount * 2;

		decoder_data(mpd_decoder, NULL, sampleBuffer,
			     sampleBufferLen, file_time,
			     bitRate, NULL);
	} while (decoder_get_command(mpd_decoder) == DECODE_COMMAND_NONE);

	faacDecClose(decoder);
	if (b.buffer)
		free(b.buffer);
}

static struct tag *aacTagDup(const char *file)
{
	struct tag *ret = NULL;
	int file_time = getAacTotalTime(file);

	if (file_time >= 0) {
		if ((ret = tag_id3_load(file)) == NULL)
			ret = tag_new();
		ret->time = file_time;
	} else {
		DEBUG("aacTagDup: Failed to get total song time from: %s\n",
		      file);
	}

	return ret;
}

static const char *const aac_suffixes[] = { "aac", NULL };
static const char *const aac_mimeTypes[] = { "audio/aac", "audio/aacp", NULL };

const struct decoder_plugin aacPlugin = {
	.name = "aac",
	.stream_decode = aac_stream_decode,
	.file_decode = aac_decode,
	.tag_dup = aacTagDup,
	.suffixes = aac_suffixes,
	.mime_types = aac_mimeTypes
};
