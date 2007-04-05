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

#include "../inputPlugin.h"

#ifdef HAVE_FAAD

#define AAC_MAX_CHANNELS	6

#include "../utils.h"
#include "../audio.h"
#include "../log.h"
#include "../inputStream.h"
#include "../outputBuffer.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <faad.h>

/* all code here is either based on or copied from FAAD2's frontend code */
typedef struct {
	InputStream *inStream;
	long bytesIntoBuffer;
	long bytesConsumed;
	long fileOffset;
	unsigned char *buffer;
	int atEof;
} AacBuffer;

static void fillAacBuffer(AacBuffer * b)
{
	if (b->bytesConsumed > 0) {
		int bread;

		if (b->bytesIntoBuffer) {
			memmove((void *)b->buffer, (void *)(b->buffer +
							    b->bytesConsumed),
				b->bytesIntoBuffer);
		}

		if (!b->atEof) {
			bread = readFromInputStream(b->inStream,
						    (void *)(b->buffer +
							     b->
							     bytesIntoBuffer),
						    1, b->bytesConsumed);
			if (bread != b->bytesConsumed)
				b->atEof = 1;
			b->bytesIntoBuffer += bread;
		}

		b->bytesConsumed = 0;

		if (b->bytesIntoBuffer > 3) {
			if (memcmp(b->buffer, "TAG", 3) == 0)
				b->bytesIntoBuffer = 0;
		}
		if (b->bytesIntoBuffer > 11) {
			if (memcmp(b->buffer, "LYRICSBEGIN", 11) == 0) {
				b->bytesIntoBuffer = 0;
			}
		}
		if (b->bytesIntoBuffer > 8) {
			if (memcmp(b->buffer, "APETAGEX", 8) == 0) {
				b->bytesIntoBuffer = 0;
			}
		}
	}
}

static void advanceAacBuffer(AacBuffer * b, int bytes)
{
	b->fileOffset += bytes;
	b->bytesConsumed = bytes;
	b->bytesIntoBuffer -= bytes;
}

static int adtsSampleRates[] =
    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

static int adtsParse(AacBuffer * b, float *length)
{
	int frames, frameLength;
	int tFrameLength = 0;
	int sampleRate = 0;
	float framesPerSec, bytesPerFrame;

	/* Read all frames to ensure correct time and bitrate */
	for (frames = 0;; frames++) {
		fillAacBuffer(b);

		if (b->bytesIntoBuffer > 7) {
			/* check syncword */
			if (!((b->buffer[0] == 0xFF) &&
			      ((b->buffer[1] & 0xF6) == 0xF0))) {
				break;
			}

			if (frames == 0) {
				sampleRate = adtsSampleRates[(b->
							      buffer[2] & 0x3c)
							     >> 2];
			}

			frameLength = ((((unsigned int)b->buffer[3] & 0x3))
				       << 11) | (((unsigned int)b->buffer[4])
						 << 3) | (b->buffer[5] >> 5);

			tFrameLength += frameLength;

			if (frameLength > b->bytesIntoBuffer)
				break;

			advanceAacBuffer(b, frameLength);
		} else
			break;
	}

	framesPerSec = (float)sampleRate / 1024.0;
	if (frames != 0) {
		bytesPerFrame = (float)tFrameLength / (float)(frames * 1000);
	} else
		bytesPerFrame = 0;
	if (framesPerSec != 0)
		*length = (float)frames / framesPerSec;

	return 1;
}

static void initAacBuffer(InputStream * inStream, AacBuffer * b, float *length,
			  size_t * retFileread, size_t * retTagsize)
{
	size_t fileread;
	size_t bread;
	size_t tagsize;

	if (length)
		*length = -1;

	memset(b, 0, sizeof(AacBuffer));

	b->inStream = inStream;

	fileread = inStream->size;

	b->buffer = xmalloc(FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS);
	memset(b->buffer, 0, FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS);

	bread = readFromInputStream(inStream, b->buffer, 1,
				    FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS);
	b->bytesIntoBuffer = bread;
	b->bytesConsumed = 0;
	b->fileOffset = 0;

	if (bread != FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS)
		b->atEof = 1;

	tagsize = 0;
	if (!memcmp(b->buffer, "ID3", 3)) {
		tagsize = (b->buffer[6] << 21) | (b->buffer[7] << 14) |
		    (b->buffer[8] << 7) | (b->buffer[9] << 0);

		tagsize += 10;
		advanceAacBuffer(b, tagsize);
		fillAacBuffer(b);
	}

	if (retFileread)
		*retFileread = fileread;
	if (retTagsize)
		*retTagsize = tagsize;

	if (length == NULL)
		return;

	if ((b->buffer[0] == 0xFF) && ((b->buffer[1] & 0xF6) == 0xF0)) {
		adtsParse(b, length);
		seekInputStream(b->inStream, tagsize, SEEK_SET);

		bread = readFromInputStream(b->inStream, b->buffer, 1,
					    FAAD_MIN_STREAMSIZE *
					    AAC_MAX_CHANNELS);
		if (bread != FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS)
			b->atEof = 1;
		else
			b->atEof = 0;
		b->bytesIntoBuffer = bread;
		b->bytesConsumed = 0;
		b->fileOffset = tagsize;
	} else if (memcmp(b->buffer, "ADIF", 4) == 0) {
		int bitRate;
		int skipSize = (b->buffer[4] & 0x80) ? 9 : 0;
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

static float getAacFloatTotalTime(char *file)
{
	AacBuffer b;
	float length;
	size_t fileread, tagsize;
	faacDecHandle decoder;
	faacDecConfigurationPtr config;
	unsigned long sampleRate;
	unsigned char channels;
	InputStream inStream;
	long bread;

	if (openInputStream(&inStream, file) < 0)
		return -1;

	initAacBuffer(&inStream, &b, &length, &fileread, &tagsize);

	if (length < 0) {
		decoder = faacDecOpen();

		config = faacDecGetCurrentConfiguration(decoder);
		config->outputFormat = FAAD_FMT_16BIT;
		faacDecSetConfiguration(decoder, config);

		fillAacBuffer(&b);
#ifdef HAVE_FAAD_BUFLEN_FUNCS
		bread = faacDecInit(decoder, b.buffer, b.bytesIntoBuffer,
				    &sampleRate, &channels);
#else
		bread = faacDecInit(decoder, b.buffer, &sampleRate, &channels);
#endif
		if (bread >= 0 && sampleRate > 0 && channels > 0)
			length = 0;

		faacDecClose(decoder);
	}

	if (b.buffer)
		free(b.buffer);
	closeInputStream(&inStream);

	return length;
}

static int getAacTotalTime(char *file)
{
	int time = -1;
	float length;

	if ((length = getAacFloatTotalTime(file)) >= 0)
		time = length + 0.5;

	return time;
}

static int aac_decode(OutputBuffer * cb, DecoderControl * dc, char *path)
{
	float time;
	float totalTime;
	faacDecHandle decoder;
	faacDecFrameInfo frameInfo;
	faacDecConfigurationPtr config;
	long bread;
	unsigned long sampleRate;
	unsigned char channels;
	int eof = 0;
	unsigned int sampleCount;
	char *sampleBuffer;
	size_t sampleBufferLen;
	/*float * seekTable;
	   long seekTableEnd = -1;
	   int seekPositionFound = 0; */
	mpd_uint16 bitRate = 0;
	AacBuffer b;
	InputStream inStream;

	if ((totalTime = getAacFloatTotalTime(path)) < 0)
		return -1;

	if (openInputStream(&inStream, path) < 0)
		return -1;

	initAacBuffer(&inStream, &b, NULL, NULL, NULL);

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
			    &sampleRate, &channels);
#else
	bread = faacDecInit(decoder, b.buffer, &sampleRate, &channels);
#endif
	if (bread < 0) {
		ERROR("Error not a AAC stream.\n");
		faacDecClose(decoder);
		closeInputStream(b.inStream);
		if (b.buffer)
			free(b.buffer);
		return -1;
	}

	dc->audioFormat.bits = 16;

	dc->totalTime = totalTime;

	time = 0.0;

	advanceAacBuffer(&b, bread);

	while (!eof) {
		fillAacBuffer(&b);

		if (b.bytesIntoBuffer == 0) {
			eof = 1;
			break;
		}
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
			eof = 1;
			break;
		}
#ifdef HAVE_FAACDECFRAMEINFO_SAMPLERATE
		sampleRate = frameInfo.samplerate;
#endif

		if (dc->state != DECODE_STATE_DECODE) {
			dc->audioFormat.channels = frameInfo.channels;
			dc->audioFormat.sampleRate = sampleRate;
			getOutputAudioFormat(&(dc->audioFormat),
					     &(cb->audioFormat));
			dc->state = DECODE_STATE_DECODE;
		}

		advanceAacBuffer(&b, frameInfo.bytesconsumed);

		sampleCount = (unsigned long)(frameInfo.samples);

		if (sampleCount > 0) {
			bitRate = frameInfo.bytesconsumed * 8.0 *
			    frameInfo.channels * sampleRate /
			    frameInfo.samples / 1000 + 0.5;
			time +=
			    (float)(frameInfo.samples) / frameInfo.channels /
			    sampleRate;
		}

		sampleBufferLen = sampleCount * 2;

		sendDataToOutputBuffer(cb, NULL, dc, 0, sampleBuffer,
				       sampleBufferLen, time, bitRate, NULL);
		if (dc->seek) {
			dc->seekError = 1;
			dc->seek = 0;
		} else if (dc->stop) {
			eof = 1;
			break;
		}
	}

	flushOutputBuffer(cb);

	faacDecClose(decoder);
	closeInputStream(b.inStream);
	if (b.buffer)
		free(b.buffer);

	if (dc->state != DECODE_STATE_DECODE)
		return -1;

	if (dc->seek) {
		dc->seekError = 1;
		dc->seek = 0;
	}

	if (dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	} else
		dc->state = DECODE_STATE_STOP;

	return 0;
}

static MpdTag *aacTagDup(char *file)
{
	MpdTag *ret = NULL;
	int time;

	time = getAacTotalTime(file);

	if (time >= 0) {
		if ((ret = id3Dup(file)) == NULL)
			ret = newMpdTag();
		ret->time = time;
	} else {
		DEBUG("aacTagDup: Failed to get total song time from: %s\n",
		      file);
	}

	return ret;
}

static char *aacSuffixes[] = { "aac", NULL };

InputPlugin aacPlugin = {
	"aac",
	NULL,
	NULL,
	NULL,
	NULL,
	aac_decode,
	aacTagDup,
	INPUT_PLUGIN_STREAM_FILE,
	aacSuffixes,
	NULL
};

#else

InputPlugin aacPlugin;

#endif /* HAVE_FAAD */
