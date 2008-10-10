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

#ifdef HAVE_MAD

#include <mad.h>

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

#include "../log.h"
#include "../utils.h"
#include "../conf.h"

#define FRAMES_CUSHION    2000

#define READ_BUFFER_SIZE  40960

enum mp3_action {
	DECODE_SKIP = -3,
	DECODE_BREAK = -2,
	DECODE_CONT = -1,
	DECODE_OK = 0
};

enum muteframe {
	MUTEFRAME_NONE,
	MUTEFRAME_SKIP,
	MUTEFRAME_SEEK
};

/* the number of samples of silence the decoder inserts at start */
#define DECODERDELAY 529

#define DEFAULT_GAPLESS_MP3_PLAYBACK 1

static int gaplessPlaybackEnabled;

/* this is stolen from mpg321! */
struct audio_dither {
	mad_fixed_t error[3];
	mad_fixed_t random;
};

static unsigned long prng(unsigned long state)
{
	return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

static int16_t audio_linear_dither(mad_fixed_t sample,
				   struct audio_dither *dither)
{
	mad_fixed_t output, mask, rnd;

	enum {
		bits = 16,
		scalebits = MAD_F_FRACBITS + 1 - bits,
		MIN = -MAD_F_ONE,
		MAX = MAD_F_ONE - 1
	};

	sample += dither->error[0] - dither->error[1] + dither->error[2];

	dither->error[2] = dither->error[1];
	dither->error[1] = dither->error[0] / 2;

	output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

	mask = (1L << scalebits) - 1;

	rnd = prng(dither->random);
	output += (rnd & mask) - (dither->random & mask);

	dither->random = rnd;

	if (output > MAX) {
		output = MAX;

		if (sample > MAX)
			sample = MAX;
	} else if (output < MIN) {
		output = MIN;

		if (sample < MIN)
			sample = MIN;
	}

	output &= ~mask;

	dither->error[0] = sample - output;

	return (int16_t)(output >> scalebits);
}

static unsigned dither_buffer(int16_t *dest0, const struct mad_synth *synth,
			      struct audio_dither *dither,
			      unsigned int start, unsigned int end,
			      unsigned int num_channels)
{
	int16_t *dest = dest0;
	unsigned int i, c;

	for (i = start; i < end; ++i) {
		for (c = 0; c < num_channels; ++c)
			*dest++ = audio_linear_dither(synth->pcm.samples[c][i],
						      dither);
	}

	return dest - dest0;
}

/* end of stolen stuff from mpg321 */

static int mp3_plugin_init(void)
{
	gaplessPlaybackEnabled = getBoolConfigParam(CONF_GAPLESS_MP3_PLAYBACK,
	                                            1);
	if (gaplessPlaybackEnabled == CONF_BOOL_UNSET)
		gaplessPlaybackEnabled = DEFAULT_GAPLESS_MP3_PLAYBACK;
	return 1;
}

/* decoder stuff is based on madlld */

#define MP3_DATA_OUTPUT_BUFFER_SIZE 2048

typedef struct _mp3DecodeData {
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
	mad_timer_t timer;
	unsigned char readBuffer[READ_BUFFER_SIZE];
	int16_t outputBuffer[MP3_DATA_OUTPUT_BUFFER_SIZE];
	float totalTime;
	float elapsedTime;
	enum muteframe muteFrame;
	long *frameOffset;
	mad_timer_t *times;
	unsigned long highestFrame;
	unsigned long maxFrames;
	unsigned long currentFrame;
	unsigned int dropFramesAtStart;
	unsigned int dropFramesAtEnd;
	unsigned int dropSamplesAtStart;
	unsigned int dropSamplesAtEnd;
	int foundXing;
	int foundFirstFrame;
	int decodedFirstFrame;
	unsigned long bitRate;
	struct decoder *decoder;
	InputStream *inStream;
	struct audio_dither dither;
	enum mad_layer layer;
} mp3DecodeData;

static void initMp3DecodeData(mp3DecodeData * data, struct decoder *decoder,
			      InputStream * inStream)
{
	data->muteFrame = MUTEFRAME_NONE;
	data->highestFrame = 0;
	data->maxFrames = 0;
	data->frameOffset = NULL;
	data->times = NULL;
	data->currentFrame = 0;
	data->dropFramesAtStart = 0;
	data->dropFramesAtEnd = 0;
	data->dropSamplesAtStart = 0;
	data->dropSamplesAtEnd = 0;
	data->foundXing = 0;
	data->foundFirstFrame = 0;
	data->decodedFirstFrame = 0;
	data->decoder = decoder;
	data->inStream = inStream;
	data->layer = 0;
	memset(&(data->dither), 0, sizeof(struct audio_dither));

	mad_stream_init(&data->stream);
	mad_stream_options(&data->stream, MAD_OPTION_IGNORECRC);
	mad_frame_init(&data->frame);
	mad_synth_init(&data->synth);
	mad_timer_reset(&data->timer);
}

static int seekMp3InputBuffer(mp3DecodeData * data, long offset)
{
	if (seekInputStream(data->inStream, offset, SEEK_SET) < 0) {
		return -1;
	}

	mad_stream_buffer(&data->stream, data->readBuffer, 0);
	(data->stream).error = 0;

	return 0;
}

static int fillMp3InputBuffer(mp3DecodeData * data)
{
	size_t readSize;
	size_t remaining;
	size_t readed;
	unsigned char *readStart;

	if ((data->stream).next_frame != NULL) {
		remaining = (data->stream).bufend - (data->stream).next_frame;
		memmove(data->readBuffer, (data->stream).next_frame, remaining);
		readStart = (data->readBuffer) + remaining;
		readSize = READ_BUFFER_SIZE - remaining;
	} else {
		readSize = READ_BUFFER_SIZE;
		readStart = data->readBuffer, remaining = 0;
	}

	/* we've exhausted the read buffer, so give up!, these potential
	 * mp3 frames are way too big, and thus unlikely to be mp3 frames */
	if (readSize == 0)
		return -1;

	readed = decoder_read(data->decoder, data->inStream,
			      readStart, readSize);
	if (readed == 0)
		return -1;

	mad_stream_buffer(&data->stream, data->readBuffer, readed + remaining);
	(data->stream).error = 0;

	return 0;
}

#ifdef HAVE_ID3TAG
static ReplayGainInfo *parseId3ReplayGainInfo(struct id3_tag *tag)
{
	int i;
	char *key;
	char *value;
	struct id3_frame *frame;
	int found = 0;
	ReplayGainInfo *replayGainInfo;

	replayGainInfo = newReplayGainInfo();

	for (i = 0; (frame = id3_tag_findframe(tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		key = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[1]));
		value = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[2]));

		if (strcasecmp(key, "replaygain_track_gain") == 0) {
			replayGainInfo->trackGain = atof(value);
			found = 1;
		} else if (strcasecmp(key, "replaygain_album_gain") == 0) {
			replayGainInfo->albumGain = atof(value);
			found = 1;
		} else if (strcasecmp(key, "replaygain_track_peak") == 0) {
			replayGainInfo->trackPeak = atof(value);
			found = 1;
		} else if (strcasecmp(key, "replaygain_album_peak") == 0) {
			replayGainInfo->albumPeak = atof(value);
			found = 1;
		}

		free(key);
		free(value);
	}

	if (found)
		return replayGainInfo;
	freeReplayGainInfo(replayGainInfo);
	return NULL;
}
#endif

#ifdef HAVE_ID3TAG
static void mp3_parseId3Tag(mp3DecodeData * data, size_t tagsize,
			    struct tag ** mpdTag, ReplayGainInfo ** replayGainInfo)
{
	struct id3_tag *id3Tag = NULL;
	id3_length_t count;
	id3_byte_t const *id3_data;
	id3_byte_t *allocated = NULL;
	struct tag *tmpMpdTag;
	ReplayGainInfo *tmpReplayGainInfo;

	count = data->stream.bufend - data->stream.this_frame;

	if (tagsize <= count) {
		id3_data = data->stream.this_frame;
		mad_stream_skip(&(data->stream), tagsize);
	} else {
		allocated = xmalloc(tagsize);
		if (!allocated)
			goto fail;

		memcpy(allocated, data->stream.this_frame, count);
		mad_stream_skip(&(data->stream), count);

		while (count < tagsize) {
			size_t len;

			len = decoder_read(data->decoder, data->inStream,
					   allocated + count, tagsize - count);
			if (len == 0)
				break;
			else
				count += len;
		}

		if (count != tagsize) {
			DEBUG("mp3_decode: error parsing ID3 tag\n");
			goto fail;
		}

		id3_data = allocated;
	}

	id3Tag = id3_tag_parse(id3_data, tagsize);
	if (!id3Tag)
		goto fail;

	if (mpdTag) {
		tmpMpdTag = tag_id3_import(id3Tag);
		if (tmpMpdTag) {
			if (*mpdTag)
				tag_free(*mpdTag);
			*mpdTag = tmpMpdTag;
		}
	}

	if (replayGainInfo) {
		tmpReplayGainInfo = parseId3ReplayGainInfo(id3Tag);
		if (tmpReplayGainInfo) {
			if (*replayGainInfo)
				freeReplayGainInfo(*replayGainInfo);
			*replayGainInfo = tmpReplayGainInfo;
		}
	}

	id3_tag_delete(id3Tag);
fail:
	if (allocated)
		free(allocated);
}
#endif

static enum mp3_action
decodeNextFrameHeader(mp3DecodeData * data, struct tag ** tag,
		      ReplayGainInfo ** replayGainInfo)
{
	enum mad_layer layer;

	if ((data->stream).buffer == NULL
	    || (data->stream).error == MAD_ERROR_BUFLEN) {
		if (fillMp3InputBuffer(data) < 0) {
			return DECODE_BREAK;
		}
	}
	if (mad_header_decode(&data->frame.header, &data->stream)) {
#ifdef HAVE_ID3TAG
		if ((data->stream).error == MAD_ERROR_LOSTSYNC &&
		    (data->stream).this_frame) {
			signed long tagsize = id3_tag_query((data->stream).
							    this_frame,
							    (data->stream).
							    bufend -
							    (data->stream).
							    this_frame);

			if (tagsize > 0) {
				if (tag && !(*tag)) {
					mp3_parseId3Tag(data, (size_t)tagsize,
							tag, replayGainInfo);
				} else {
					mad_stream_skip(&(data->stream),
							tagsize);
				}
				return DECODE_CONT;
			}
		}
#endif
		if (MAD_RECOVERABLE((data->stream).error)) {
			return DECODE_SKIP;
		} else {
			if ((data->stream).error == MAD_ERROR_BUFLEN)
				return DECODE_CONT;
			else {
				ERROR("unrecoverable frame level error "
				      "(%s).\n",
				      mad_stream_errorstr(&data->stream));
				return DECODE_BREAK;
			}
		}
	}

	layer = data->frame.header.layer;
	if (!data->layer) {
		if (layer != MAD_LAYER_II && layer != MAD_LAYER_III) {
			/* Only layer 2 and 3 have been tested to work */
			return DECODE_SKIP;
		}
		data->layer = layer;
	} else if (layer != data->layer) {
		/* Don't decode frames with a different layer than the first */
		return DECODE_SKIP;
	}

	return DECODE_OK;
}

static enum mp3_action
decodeNextFrame(mp3DecodeData * data)
{
	if ((data->stream).buffer == NULL
	    || (data->stream).error == MAD_ERROR_BUFLEN) {
		if (fillMp3InputBuffer(data) < 0) {
			return DECODE_BREAK;
		}
	}
	if (mad_frame_decode(&data->frame, &data->stream)) {
#ifdef HAVE_ID3TAG
		if ((data->stream).error == MAD_ERROR_LOSTSYNC) {
			signed long tagsize = id3_tag_query((data->stream).
							    this_frame,
							    (data->stream).
							    bufend -
							    (data->stream).
							    this_frame);
			if (tagsize > 0) {
				mad_stream_skip(&(data->stream), tagsize);
				return DECODE_CONT;
			}
		}
#endif
		if (MAD_RECOVERABLE((data->stream).error)) {
			return DECODE_SKIP;
		} else {
			if ((data->stream).error == MAD_ERROR_BUFLEN)
				return DECODE_CONT;
			else {
				ERROR("unrecoverable frame level error "
				      "(%s).\n",
				      mad_stream_errorstr(&data->stream));
				return DECODE_BREAK;
			}
		}
	}

	return DECODE_OK;
}

/* xing stuff stolen from alsaplayer, and heavily modified by jat */
#define XI_MAGIC (('X' << 8) | 'i')
#define NG_MAGIC (('n' << 8) | 'g')
#define IN_MAGIC (('I' << 8) | 'n')
#define FO_MAGIC (('f' << 8) | 'o')

enum xing_magic {
	XING_MAGIC_XING, /* VBR */
	XING_MAGIC_INFO  /* CBR */
};

struct xing {
	long flags;             /* valid fields (see below) */
	unsigned long frames;   /* total number of frames */
	unsigned long bytes;    /* total number of bytes */
	unsigned char toc[100]; /* 100-point seek table */
	long scale;             /* VBR quality */
	enum xing_magic magic;  /* header magic */
};

enum {
	XING_FRAMES = 0x00000001L,
	XING_BYTES  = 0x00000002L,
	XING_TOC    = 0x00000004L,
	XING_SCALE  = 0x00000008L
};

struct version {
	unsigned major;
	unsigned minor;
};

struct lame {
	char encoder[10];       /* 9 byte encoder name/version ("LAME3.97b") */
	struct version version; /* struct containing just the version */
	float peak;             /* replaygain peak */
	float trackGain;        /* replaygain track gain */
	float albumGain;        /* replaygain album gain */
	int encoderDelay;       /* # of added samples at start of mp3 */
	int encoderPadding;     /* # of added samples at end of mp3 */
	int crc;                /* CRC of the first 190 bytes of this frame */
};

static int parse_xing(struct xing *xing, struct mad_bitptr *ptr, int *oldbitlen)
{
	unsigned long bits;
	int bitlen;
	int bitsleft;
	int i;

	bitlen = *oldbitlen;

	if (bitlen < 16) goto fail;
	bits = mad_bit_read(ptr, 16);
	bitlen -= 16;

	if (bits == XI_MAGIC) {
		if (bitlen < 16) goto fail;
		if (mad_bit_read(ptr, 16) != NG_MAGIC) goto fail;
		bitlen -= 16;
		xing->magic = XING_MAGIC_XING;
	} else if (bits == IN_MAGIC) {
		if (bitlen < 16) goto fail;
		if (mad_bit_read(ptr, 16) != FO_MAGIC) goto fail;
		bitlen -= 16;
		xing->magic = XING_MAGIC_INFO;
	}
	else if (bits == NG_MAGIC) xing->magic = XING_MAGIC_XING;
	else if (bits == FO_MAGIC) xing->magic = XING_MAGIC_INFO;
	else goto fail;

	if (bitlen < 32) goto fail;
	xing->flags = mad_bit_read(ptr, 32);
	bitlen -= 32;

	if (xing->flags & XING_FRAMES) {
		if (bitlen < 32) goto fail;
		xing->frames = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	if (xing->flags & XING_BYTES) {
		if (bitlen < 32) goto fail;
		xing->bytes = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	if (xing->flags & XING_TOC) {
		if (bitlen < 800) goto fail;
		for (i = 0; i < 100; ++i) xing->toc[i] = mad_bit_read(ptr, 8);
		bitlen -= 800;
	}

	if (xing->flags & XING_SCALE) {
		if (bitlen < 32) goto fail;
		xing->scale = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	/* Make sure we consume no less than 120 bytes (960 bits) in hopes that
	 * the LAME tag is found there, and not right after the Xing header */
	bitsleft = 960 - ((*oldbitlen) - bitlen);
	if (bitsleft < 0) goto fail;
	else if (bitsleft > 0) {
		mad_bit_read(ptr, bitsleft);
		bitlen -= bitsleft;
	}

	*oldbitlen = bitlen;

	return 1;
fail:
	xing->flags = 0;
	return 0;
}

static int parse_lame(struct lame *lame, struct mad_bitptr *ptr, int *bitlen)
{
	int adj = 0;
	int name;
	int orig;
	int sign;
	int gain;
	int i;

	/* Unlike the xing header, the lame tag has a fixed length.  Fail if
	 * not all 36 bytes (288 bits) are there. */
	if (*bitlen < 288)
		return 0;

	for (i = 0; i < 9; i++)
		lame->encoder[i] = (char)mad_bit_read(ptr, 8);
	lame->encoder[9] = '\0';

	*bitlen -= 72;

	/* This is technically incorrect, since the encoder might not be lame.
	 * But there's no other way to determine if this is a lame tag, and we
	 * wouldn't want to go reading a tag that's not there. */
	if (prefixcmp(lame->encoder, "LAME"))
		return 0;

	if (sscanf(lame->encoder+4, "%u.%u",
	           &lame->version.major, &lame->version.minor) != 2)
		return 0;

	DEBUG("detected LAME version %i.%i (\"%s\")\n",
	      lame->version.major, lame->version.minor, lame->encoder);

	/* The reference volume was changed from the 83dB used in the
	 * ReplayGain spec to 89dB in lame 3.95.1.  Bump the gain for older
	 * versions, since everyone else uses 89dB instead of 83dB.
	 * Unfortunately, lame didn't differentiate between 3.95 and 3.95.1, so
	 * it's impossible to make the proper adjustment for 3.95.
	 * Fortunately, 3.95 was only out for about a day before 3.95.1 was
	 * released. -- tmz */
	if (lame->version.major < 3 ||
	    (lame->version.major == 3 && lame->version.minor < 95))
		adj = 6;

	mad_bit_read(ptr, 16);

	lame->peak = mad_f_todouble(mad_bit_read(ptr, 32) << 5); /* peak */
	DEBUG("LAME peak found: %f\n", lame->peak);

	lame->trackGain = 0;
	name = mad_bit_read(ptr, 3); /* gain name */
	orig = mad_bit_read(ptr, 3); /* gain originator */
	sign = mad_bit_read(ptr, 1); /* sign bit */
	gain = mad_bit_read(ptr, 9); /* gain*10 */
	if (gain && name == 1 && orig != 0) {
		lame->trackGain = ((sign ? -gain : gain) / 10.0) + adj;
		DEBUG("LAME track gain found: %f\n", lame->trackGain);
	}

	/* tmz reports that this isn't currently written by any version of lame
	 * (as of 3.97).  Since we have no way of testing it, don't use it.
	 * Wouldn't want to go blowing someone's ears just because we read it
	 * wrong. :P -- jat */
	lame->albumGain = 0;
#if 0
	name = mad_bit_read(ptr, 3); /* gain name */
	orig = mad_bit_read(ptr, 3); /* gain originator */
	sign = mad_bit_read(ptr, 1); /* sign bit */
	gain = mad_bit_read(ptr, 9); /* gain*10 */
	if (gain && name == 2 && orig != 0) {
		lame->albumGain = ((sign ? -gain : gain) / 10.0) + adj;
		DEBUG("LAME album gain found: %f\n", lame->trackGain);
	}
#else
	mad_bit_read(ptr, 16);
#endif

	mad_bit_read(ptr, 16);

	lame->encoderDelay = mad_bit_read(ptr, 12);
	lame->encoderPadding = mad_bit_read(ptr, 12);

	DEBUG("encoder delay is %i, encoder padding is %i\n",
	      lame->encoderDelay, lame->encoderPadding);

	mad_bit_read(ptr, 80);

	lame->crc = mad_bit_read(ptr, 16);

	*bitlen -= 216;

	return 1;
}

static int decodeFirstFrame(mp3DecodeData * data,
                            struct tag ** tag, ReplayGainInfo ** replayGainInfo)
{
	struct decoder *decoder = data->decoder;
	struct xing xing;
	struct lame lame;
	struct mad_bitptr ptr;
	int bitlen;
	int ret;

	/* stfu gcc */
	memset(&xing, 0, sizeof(struct xing));
	xing.flags = 0;

	while (1) {
		while ((ret = decodeNextFrameHeader(data, tag, replayGainInfo)) == DECODE_CONT &&
		       (!decoder || decoder_get_command(decoder) == DECODE_COMMAND_NONE));
		if (ret == DECODE_BREAK ||
		    (decoder && decoder_get_command(decoder) != DECODE_COMMAND_NONE))
			return -1;
		if (ret == DECODE_SKIP) continue;

		while ((ret = decodeNextFrame(data)) == DECODE_CONT &&
		       (!decoder || decoder_get_command(decoder) == DECODE_COMMAND_NONE));
		if (ret == DECODE_BREAK ||
		    (decoder && decoder_get_command(decoder) != DECODE_COMMAND_NONE))
			return -1;
		if (ret == DECODE_OK) break;
	}

	ptr = data->stream.anc_ptr;
	bitlen = data->stream.anc_bitlen;

	/*
	 * Attempt to calulcate the length of the song from filesize
	 */
	{
		size_t offset = data->inStream->offset;
		mad_timer_t duration = data->frame.header.duration;
		float frameTime = ((float)mad_timer_count(duration,
		                   MAD_UNITS_MILLISECONDS)) / 1000;

		if (data->stream.this_frame != NULL)
			offset -= data->stream.bufend - data->stream.this_frame;
		else
			offset -= data->stream.bufend - data->stream.buffer;

		if (data->inStream->size >= offset) {
			data->totalTime = ((data->inStream->size - offset) *
			                   8.0) / (data->frame).header.bitrate;
			data->maxFrames = data->totalTime / frameTime +
			                  FRAMES_CUSHION;
		} else {
			data->maxFrames = FRAMES_CUSHION;
			data->totalTime = 0;
		}
	}
	/*
	 * if an xing tag exists, use that!
	 */
	if (parse_xing(&xing, &ptr, &bitlen)) {
		data->foundXing = 1;
		data->muteFrame = MUTEFRAME_SKIP;

		if ((xing.flags & XING_FRAMES) && xing.frames) {
			mad_timer_t duration = data->frame.header.duration;
			mad_timer_multiply(&duration, xing.frames);
			data->totalTime = ((float)mad_timer_count(duration, MAD_UNITS_MILLISECONDS)) / 1000;
			data->maxFrames = xing.frames;
		}

		if (parse_lame(&lame, &ptr, &bitlen)) {
			if (gaplessPlaybackEnabled &&
			    data->inStream->seekable) {
				data->dropSamplesAtStart = lame.encoderDelay +
				                           DECODERDELAY;
				data->dropSamplesAtEnd = lame.encoderPadding;
			}

			/* Album gain isn't currently used.  See comment in
			 * parse_lame() for details. -- jat */
			if (replayGainInfo && !*replayGainInfo &&
			    lame.trackGain) {
				*replayGainInfo = newReplayGainInfo();
				(*replayGainInfo)->trackGain = lame.trackGain;
				(*replayGainInfo)->trackPeak = lame.peak;
			}
		}
	} 

	if (!data->maxFrames) return -1;

	if (data->maxFrames > 8 * 1024 * 1024) {
		ERROR("mp3 file header indicates too many frames: %lu",
		      data->maxFrames);
		return -1;
	}

	data->frameOffset = xmalloc(sizeof(long) * data->maxFrames);
	data->times = xmalloc(sizeof(mad_timer_t) * data->maxFrames);

	return 0;
}

static void mp3DecodeDataFinalize(mp3DecodeData * data)
{
	mad_synth_finish(&data->synth);
	mad_frame_finish(&data->frame);
	mad_stream_finish(&data->stream);

	if (data->frameOffset) free(data->frameOffset);
	if (data->times) free(data->times);
}

/* this is primarily used for getting total time for tags */
static int getMp3TotalTime(char *file)
{
	InputStream inStream;
	mp3DecodeData data;
	int ret;

	if (openInputStream(&inStream, file) < 0)
		return -1;
	initMp3DecodeData(&data, NULL, &inStream);
	if (decodeFirstFrame(&data, NULL, NULL) < 0)
		ret = -1;
	else
		ret = data.totalTime + 0.5;
	mp3DecodeDataFinalize(&data);
	closeInputStream(&inStream);

	return ret;
}

static int openMp3FromInputStream(InputStream * inStream, mp3DecodeData * data,
				  struct decoder * decoder, struct tag ** tag,
				  ReplayGainInfo ** replayGainInfo)
{
	initMp3DecodeData(data, decoder, inStream);
	*tag = NULL;
	if (decodeFirstFrame(data, tag, replayGainInfo) < 0) {
		mp3DecodeDataFinalize(data);
		if (tag && *tag)
			tag_free(*tag);
		return -1;
	}

	return 0;
}

static enum mp3_action
mp3Read(mp3DecodeData * data, ReplayGainInfo ** replayGainInfo)
{
	struct decoder *decoder = data->decoder;
	unsigned int pcm_length, max_samples;
	unsigned int i;
	int ret;
	int skip;

	if (data->currentFrame >= data->highestFrame) {
		mad_timer_add(&data->timer, (data->frame).header.duration);
		data->bitRate = (data->frame).header.bitrate;
		if (data->currentFrame >= data->maxFrames) {
			data->currentFrame = data->maxFrames - 1;
		} else {
			data->highestFrame++;
		}
		data->frameOffset[data->currentFrame] = data->inStream->offset;
		if (data->stream.this_frame != NULL) {
			data->frameOffset[data->currentFrame] -=
			    data->stream.bufend - data->stream.this_frame;
		} else {
			data->frameOffset[data->currentFrame] -=
			    data->stream.bufend - data->stream.buffer;
		}
		data->times[data->currentFrame] = data->timer;
	} else {
		data->timer = data->times[data->currentFrame];
	}
	data->currentFrame++;
	data->elapsedTime =
	    ((float)mad_timer_count(data->timer, MAD_UNITS_MILLISECONDS)) /
	    1000;

	switch (data->muteFrame) {
	case MUTEFRAME_SKIP:
		data->muteFrame = MUTEFRAME_NONE;
		break;
	case MUTEFRAME_SEEK:
		if (decoder_seek_where(decoder) <= data->elapsedTime) {
			decoder_clear(decoder);
			data->muteFrame = MUTEFRAME_NONE;
			decoder_command_finished(decoder);
		}
		break;
	case MUTEFRAME_NONE:
		mad_synth_frame(&data->synth, &data->frame);

		if (!data->foundFirstFrame) {
			unsigned int samplesPerFrame = (data->synth).pcm.length;
			data->dropFramesAtStart = data->dropSamplesAtStart / samplesPerFrame;
			data->dropFramesAtEnd = data->dropSamplesAtEnd / samplesPerFrame;
			data->dropSamplesAtStart = data->dropSamplesAtStart % samplesPerFrame;
			data->dropSamplesAtEnd = data->dropSamplesAtEnd % samplesPerFrame;
			data->foundFirstFrame = 1;
		}

		if (data->dropFramesAtStart > 0) {
			data->dropFramesAtStart--;
			break;
		} else if ((data->dropFramesAtEnd > 0) && 
		           (data->currentFrame == (data->maxFrames + 1 - data->dropFramesAtEnd))) {
			/* stop decoding, effectively dropping all remaining
			 * frames */
			return DECODE_BREAK;
		}

		if (data->inStream->metaTitle) {
			struct tag *tag = tag_new();
			if (data->inStream->metaName) {
				tag_add_item(tag, TAG_ITEM_NAME,
					     data->inStream->metaName);
			}
			tag_add_item(tag, TAG_ITEM_TITLE,
					data->inStream->metaTitle);
			free(data->inStream->metaTitle);
			data->inStream->metaTitle = NULL;
			tag_free(tag);
		}

		if (!data->decodedFirstFrame) {
			i = data->dropSamplesAtStart;
			data->decodedFirstFrame = 1;
		} else
			i = 0;

		pcm_length = data->synth.pcm.length;
		if (data->dropSamplesAtEnd &&
		    (data->currentFrame == data->maxFrames - data->dropFramesAtEnd)) {
			if (data->dropSamplesAtEnd >= pcm_length)
				pcm_length = 0;
			else
				pcm_length -= data->dropSamplesAtEnd;
		}

		max_samples = sizeof(data->outputBuffer) /
			(2 * MAD_NCHANNELS(&(data->frame).header));

		while (i < pcm_length) {
			enum decoder_command cmd;
			unsigned int num_samples = pcm_length - i;
			if (num_samples > max_samples)
				num_samples = max_samples;

			i += num_samples;

			num_samples = dither_buffer(data->outputBuffer,
						    &data->synth, &data->dither,
						    i - num_samples, i,
						    MAD_NCHANNELS(&(data->frame).header));

			cmd = decoder_data(decoder, data->inStream,
					   data->inStream->seekable,
					   data->outputBuffer,
					   2 * num_samples,
					   data->elapsedTime,
					   data->bitRate / 1000,
					   (replayGainInfo != NULL) ? *replayGainInfo : NULL);
			if (cmd == DECODE_COMMAND_STOP)
				return DECODE_BREAK;
		}

		if (data->dropSamplesAtEnd &&
		    (data->currentFrame == data->maxFrames - data->dropFramesAtEnd))
			/* stop decoding, effectively dropping
			 * all remaining samples */
			return DECODE_BREAK;

		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK &&
		    data->inStream->seekable) {
			unsigned long j = 0;
			data->muteFrame = MUTEFRAME_SEEK;
			while (j < data->highestFrame &&
			       decoder_seek_where(decoder) >
			       ((float)mad_timer_count(data->times[j],
						       MAD_UNITS_MILLISECONDS))
			       / 1000) {
				j++;
			}
			if (j < data->highestFrame) {
				if (seekMp3InputBuffer(data,
						       data->frameOffset[j]) ==
				    0) {
					decoder_clear(decoder);
					data->currentFrame = j;
					decoder_command_finished(decoder);
				} else
					decoder_seek_error(decoder);
				data->muteFrame = MUTEFRAME_NONE;
			}
		} else if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK &&
			   !data->inStream->seekable) {
			decoder_seek_error(decoder);
		}
	}

	while (1) {
		skip = 0;
		while ((ret =
			decodeNextFrameHeader(data, NULL,
					      replayGainInfo)) == DECODE_CONT
		       && decoder_get_command(decoder) == DECODE_COMMAND_NONE) ;
		if (ret == DECODE_BREAK || decoder_get_command(decoder) != DECODE_COMMAND_NONE)
			break;
		else if (ret == DECODE_SKIP)
			skip = 1;
		if (data->muteFrame == MUTEFRAME_NONE) {
			while ((ret = decodeNextFrame(data)) == DECODE_CONT &&
			       decoder_get_command(decoder) == DECODE_COMMAND_NONE) ;
			if (ret == DECODE_BREAK ||
			    decoder_get_command(decoder) != DECODE_COMMAND_NONE)
				break;
		}
		if (!skip && ret == DECODE_OK)
			break;
	}

	switch (decoder_get_command(decoder)) {
	case DECODE_COMMAND_NONE:
	case DECODE_COMMAND_START:
		break;

	case DECODE_COMMAND_STOP:
		return DECODE_BREAK;

	case DECODE_COMMAND_SEEK:
		return DECODE_CONT;
	}

	return ret;
}

static void initAudioFormatFromMp3DecodeData(mp3DecodeData * data,
					     struct audio_format * af)
{
	af->bits = 16;
	af->sample_rate = (data->frame).header.samplerate;
	af->channels = MAD_NCHANNELS(&(data->frame).header);
}

static int mp3_decode(struct decoder * decoder, InputStream * inStream)
{
	mp3DecodeData data;
	struct tag *tag = NULL;
	ReplayGainInfo *replayGainInfo = NULL;
	struct audio_format audio_format;

	if (openMp3FromInputStream(inStream, &data, decoder,
				   &tag, &replayGainInfo) < 0) {
		if (decoder_get_command(decoder) == DECODE_COMMAND_NONE) {
			ERROR
			    ("Input does not appear to be a mp3 bit stream.\n");
			return -1;
		}
		return 0;
	}

	initAudioFormatFromMp3DecodeData(&data, &audio_format);

	if (inStream->metaTitle) {
		if (tag)
			tag_free(tag);
		tag = tag_new();
		tag_add_item(tag, TAG_ITEM_TITLE, inStream->metaTitle);
		free(inStream->metaTitle);
		inStream->metaTitle = NULL;
		if (inStream->metaName) {
			tag_add_item(tag, TAG_ITEM_NAME, inStream->metaName);
		}
		tag_free(tag);
	} else if (tag) {
		if (inStream->metaName) {
			tag_clear_items_by_type(tag, TAG_ITEM_NAME);
			tag_add_item(tag, TAG_ITEM_NAME, inStream->metaName);
		}
		tag_free(tag);
	} else if (inStream->metaName) {
		tag = tag_new();
		if (inStream->metaName) {
			tag_add_item(tag, TAG_ITEM_NAME, inStream->metaName);
		}
		tag_free(tag);
	}

	decoder_initialized(decoder, &audio_format, data.totalTime);

	while (mp3Read(&data, &replayGainInfo) != DECODE_BREAK) ;

	if (replayGainInfo)
		freeReplayGainInfo(replayGainInfo);

	if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK &&
	    data.muteFrame == MUTEFRAME_SEEK) {
		decoder_clear(decoder);
		decoder_command_finished(decoder);
	}

	decoder_flush(decoder);
	mp3DecodeDataFinalize(&data);

	return 0;
}

static struct tag *mp3_tagDup(char *file)
{
	struct tag *ret = NULL;
	int total_time;

	ret = tag_id3_load(file);

	total_time = getMp3TotalTime(file);

	if (total_time >= 0) {
		if (!ret)
			ret = tag_new();
		ret->time = total_time;
	} else {
		DEBUG("mp3_tagDup: Failed to get total song time from: %s\n",
		      file);
	}

	return ret;
}

static const char *mp3_suffixes[] = { "mp3", "mp2", NULL };
static const char *mp3_mimeTypes[] = { "audio/mpeg", NULL };

struct decoder_plugin mp3Plugin = {
	.name = "mp3",
	.init = mp3_plugin_init,
	.stream_decode = mp3_decode,
	.tag_dup = mp3_tagDup,
	.stream_types = INPUT_PLUGIN_STREAM_FILE | INPUT_PLUGIN_STREAM_URL,
	.suffixes = mp3_suffixes,
	.mime_types = mp3_mimeTypes
};
#else

struct decoder_plugin mp3Plugin;

#endif /* HAVE_MAD */
