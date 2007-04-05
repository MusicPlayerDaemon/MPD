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

#include "../utils.h"
#include "../audio.h"
#include "../log.h"
#include "../pcm_utils.h"
#include "../inputStream.h"
#include "../outputBuffer.h"
#include "../decode.h"

#include "../mp4ff/mp4ff.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <faad.h>

/* all code here is either based on or copied from FAAD2's frontend code */

static int mp4_getAACTrack(mp4ff_t * infile)
{
	/* find AAC track */
	int i, rc;
	int numTracks = mp4ff_total_tracks(infile);

	for (i = 0; i < numTracks; i++) {
		unsigned char *buff = NULL;
		unsigned int buff_size = 0;
#ifdef HAVE_MP4AUDIOSPECIFICCONFIG
		mp4AudioSpecificConfig mp4ASC;
#else
		unsigned long dummy1_32;
		unsigned char dummy2_8, dummy3_8, dummy4_8, dummy5_8, dummy6_8,
		    dummy7_8, dummy8_8;
#endif

		mp4ff_get_decoder_config(infile, i, &buff, &buff_size);

		if (buff) {
#ifdef HAVE_MP4AUDIOSPECIFICCONFIG
			rc = AudioSpecificConfig(buff, buff_size, &mp4ASC);
#else
			rc = AudioSpecificConfig(buff, &dummy1_32, &dummy2_8,
						 &dummy3_8, &dummy4_8,
						 &dummy5_8, &dummy6_8,
						 &dummy7_8, &dummy8_8);
#endif
			free(buff);
			if (rc < 0)
				continue;
			return i;
		}
	}

	/* can't decode this */
	return -1;
}

static uint32_t mp4_inputStreamReadCallback(void *inStream, void *buffer,
					    uint32_t length)
{
	return readFromInputStream((InputStream *) inStream, buffer, 1, length);
}

static uint32_t mp4_inputStreamSeekCallback(void *inStream, uint64_t position)
{
	return seekInputStream((InputStream *) inStream, position, SEEK_SET);
}

static int mp4_decode(OutputBuffer * cb, DecoderControl * dc, char *path)
{
	mp4ff_t *mp4fh;
	mp4ff_callback_t *mp4cb;
	int32_t track;
	float time;
	int32_t scale;
	faacDecHandle decoder;
	faacDecFrameInfo frameInfo;
	faacDecConfigurationPtr config;
	unsigned char *mp4Buffer;
	unsigned int mp4BufferSize;
	unsigned long sampleRate;
	unsigned char channels;
	long sampleId;
	long numSamples;
	int eof = 0;
	long dur;
	unsigned int sampleCount;
	char *sampleBuffer;
	size_t sampleBufferLen;
	unsigned int initial = 1;
	float *seekTable;
	long seekTableEnd = -1;
	int seekPositionFound = 0;
	long offset;
	mpd_uint16 bitRate = 0;
	InputStream inStream;
	int seeking = 0;

	if (openInputStream(&inStream, path) < 0) {
		ERROR("failed to open %s\n", path);
		return -1;
	}

	mp4cb = xmalloc(sizeof(mp4ff_callback_t));
	mp4cb->read = mp4_inputStreamReadCallback;
	mp4cb->seek = mp4_inputStreamSeekCallback;
	mp4cb->user_data = &inStream;

	mp4fh = mp4ff_open_read(mp4cb);
	if (!mp4fh) {
		ERROR("Input does not appear to be a mp4 stream.\n");
		free(mp4cb);
		closeInputStream(&inStream);
		return -1;
	}

	track = mp4_getAACTrack(mp4fh);
	if (track < 0) {
		ERROR("No AAC track found in mp4 stream.\n");
		mp4ff_close(mp4fh);
		closeInputStream(&inStream);
		free(mp4cb);
		return -1;
	}

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

	dc->audioFormat.bits = 16;

	mp4Buffer = NULL;
	mp4BufferSize = 0;
	mp4ff_get_decoder_config(mp4fh, track, &mp4Buffer, &mp4BufferSize);

	if (faacDecInit2
	    (decoder, mp4Buffer, mp4BufferSize, &sampleRate, &channels) < 0) {
		ERROR("Error not a AAC stream.\n");
		faacDecClose(decoder);
		mp4ff_close(mp4fh);
		free(mp4cb);
		closeInputStream(&inStream);
		return -1;
	}

	dc->audioFormat.sampleRate = sampleRate;
	dc->audioFormat.channels = channels;
	time = mp4ff_get_track_duration_use_offsets(mp4fh, track);
	scale = mp4ff_time_scale(mp4fh, track);

	if (mp4Buffer)
		free(mp4Buffer);

	if (scale < 0) {
		ERROR("Error getting audio format of mp4 AAC track.\n");
		faacDecClose(decoder);
		mp4ff_close(mp4fh);
		closeInputStream(&inStream);
		free(mp4cb);
		return -1;
	}
	dc->totalTime = ((float)time) / scale;

	numSamples = mp4ff_num_samples(mp4fh, track);

	time = 0.0;

	seekTable = xmalloc(sizeof(float) * numSamples);

	for (sampleId = 0; sampleId < numSamples && !eof; sampleId++) {
		if (dc->seek)
			seeking = 1;

		if (seeking && seekTableEnd > 1 &&
		    seekTable[seekTableEnd] >= dc->seekWhere) {
			int i = 2;
			while (seekTable[i] < dc->seekWhere)
				i++;
			sampleId = i - 1;
			time = seekTable[sampleId];
		}

		dur = mp4ff_get_sample_duration(mp4fh, track, sampleId);
		offset = mp4ff_get_sample_offset(mp4fh, track, sampleId);

		if (sampleId > seekTableEnd) {
			seekTable[sampleId] = time;
			seekTableEnd = sampleId;
		}

		if (sampleId == 0)
			dur = 0;
		if (offset > dur)
			dur = 0;
		else
			dur -= offset;
		time += ((float)dur) / scale;

		if (seeking && time > dc->seekWhere)
			seekPositionFound = 1;

		if (seeking && seekPositionFound) {
			seekPositionFound = 0;
			clearOutputBuffer(cb);
			seeking = 0;
			dc->seek = 0;
		}

		if (seeking)
			continue;

		if (mp4ff_read_sample(mp4fh, track, sampleId, &mp4Buffer,
				      &mp4BufferSize) == 0) {
			eof = 1;
			continue;
		}
#ifdef HAVE_FAAD_BUFLEN_FUNCS
		sampleBuffer = faacDecDecode(decoder, &frameInfo, mp4Buffer,
					     mp4BufferSize);
#else
		sampleBuffer = faacDecDecode(decoder, &frameInfo, mp4Buffer);
#endif

		if (mp4Buffer)
			free(mp4Buffer);
		if (frameInfo.error > 0) {
			ERROR("error decoding MP4 file: %s\n", path);
			ERROR("faad2 error: %s\n",
			      faacDecGetErrorMessage(frameInfo.error));
			eof = 1;
			break;
		}

		if (dc->state != DECODE_STATE_DECODE) {
			channels = frameInfo.channels;
#ifdef HAVE_FAACDECFRAMEINFO_SAMPLERATE
			scale = frameInfo.samplerate;
#endif
			dc->audioFormat.sampleRate = scale;
			dc->audioFormat.channels = frameInfo.channels;
			getOutputAudioFormat(&(dc->audioFormat),
					     &(cb->audioFormat));
			dc->state = DECODE_STATE_DECODE;
		}

		if (channels * (dur + offset) > frameInfo.samples) {
			dur = frameInfo.samples / channels;
			offset = 0;
		}

		sampleCount = (unsigned long)(dur * channels);

		if (sampleCount > 0) {
			initial = 0;
			bitRate = frameInfo.bytesconsumed * 8.0 *
			    frameInfo.channels * scale /
			    frameInfo.samples / 1000 + 0.5;
		}

		sampleBufferLen = sampleCount * 2;

		sampleBuffer += offset * channels * 2;

		sendDataToOutputBuffer(cb, NULL, dc, 1, sampleBuffer,
				       sampleBufferLen, time, bitRate, NULL);
		if (dc->stop) {
			eof = 1;
			break;
		}
	}

	free(seekTable);
	faacDecClose(decoder);
	mp4ff_close(mp4fh);
	closeInputStream(&inStream);
	free(mp4cb);

	if (dc->state != DECODE_STATE_DECODE)
		return -1;

	if (dc->seek && seeking) {
		clearOutputBuffer(cb);
		dc->seek = 0;
	}
	flushOutputBuffer(cb);

	if (dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	} else
		dc->state = DECODE_STATE_STOP;

	return 0;
}

static MpdTag *mp4DataDup(char *file, int *mp4MetadataFound)
{
	MpdTag *ret = NULL;
	InputStream inStream;
	mp4ff_t *mp4fh;
	mp4ff_callback_t *cb;
	int32_t track;
	int32_t time;
	int32_t scale;
	int i;

	*mp4MetadataFound = 0;

	if (openInputStream(&inStream, file) < 0) {
		DEBUG("mp4DataDup: Failed to open file: %s\n", file);
		return NULL;
	}

	cb = xmalloc(sizeof(mp4ff_callback_t));
	cb->read = mp4_inputStreamReadCallback;
	cb->seek = mp4_inputStreamSeekCallback;
	cb->user_data = &inStream;

	mp4fh = mp4ff_open_read(cb);
	if (!mp4fh) {
		free(cb);
		closeInputStream(&inStream);
		return NULL;
	}

	track = mp4_getAACTrack(mp4fh);
	if (track < 0) {
		mp4ff_close(mp4fh);
		closeInputStream(&inStream);
		free(cb);
		return NULL;
	}

	ret = newMpdTag();
	time = mp4ff_get_track_duration_use_offsets(mp4fh, track);
	scale = mp4ff_time_scale(mp4fh, track);
	if (scale < 0) {
		mp4ff_close(mp4fh);
		closeInputStream(&inStream);
		free(cb);
		freeMpdTag(ret);
		return NULL;
	}
	ret->time = ((float)time) / scale + 0.5;

	for (i = 0; i < mp4ff_meta_get_num_items(mp4fh); i++) {
		char *item;
		char *value;

		mp4ff_meta_get_by_index(mp4fh, i, &item, &value);

		if (0 == strcasecmp("artist", item)) {
			addItemToMpdTag(ret, TAG_ITEM_ARTIST, value);
			*mp4MetadataFound = 1;
		} else if (0 == strcasecmp("title", item)) {
			addItemToMpdTag(ret, TAG_ITEM_TITLE, value);
			*mp4MetadataFound = 1;
		} else if (0 == strcasecmp("album", item)) {
			addItemToMpdTag(ret, TAG_ITEM_ALBUM, value);
			*mp4MetadataFound = 1;
		} else if (0 == strcasecmp("track", item)) {
			addItemToMpdTag(ret, TAG_ITEM_TRACK, value);
			*mp4MetadataFound = 1;
		} else if (0 == strcasecmp("disc", item)) {	/* Is that the correct id? */
			addItemToMpdTag(ret, TAG_ITEM_DISC, value);
			*mp4MetadataFound = 1;
		} else if (0 == strcasecmp("genre", item)) {
			addItemToMpdTag(ret, TAG_ITEM_GENRE, value);
			*mp4MetadataFound = 1;
		} else if (0 == strcasecmp("date", item)) {
			addItemToMpdTag(ret, TAG_ITEM_DATE, value);
			*mp4MetadataFound = 1;
		}

		free(item);
		free(value);
	}

	mp4ff_close(mp4fh);
	closeInputStream(&inStream);
	free(cb);

	return ret;
}

static MpdTag *mp4TagDup(char *file)
{
	MpdTag *ret = NULL;
	int mp4MetadataFound = 0;

	ret = mp4DataDup(file, &mp4MetadataFound);
	if (!ret)
		return NULL;
	if (!mp4MetadataFound) {
		MpdTag *temp = id3Dup(file);
		if (temp) {
			temp->time = ret->time;
			freeMpdTag(ret);
			ret = temp;
		}
	}

	return ret;
}

static char *mp4Suffixes[] = { "m4a", "mp4", NULL };

InputPlugin mp4Plugin = {
	"mp4",
	NULL,
	NULL,
	NULL,
	NULL,
	mp4_decode,
	mp4TagDup,
	INPUT_PLUGIN_STREAM_FILE,
	mp4Suffixes,
	NULL
};

#else

InputPlugin mp4Plugin;

#endif /* HAVE_FAAD */
