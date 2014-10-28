/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "MadDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "config/ConfigGlobal.hxx"
#include "tag/TagId3.hxx"
#include "tag/TagRva2.hxx"
#include "tag/TagHandler.hxx"
#include "tag/ReplayGain.hxx"
#include "tag/MixRamp.hxx"
#include "CheckAudioFormat.hxx"
#include "util/StringUtil.hxx"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <mad.h>

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static constexpr unsigned long FRAMES_CUSHION = 2000;

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
static constexpr unsigned DECODERDELAY = 529;

static constexpr bool DEFAULT_GAPLESS_MP3_PLAYBACK = true;

static constexpr Domain mad_domain("mad");

static bool gapless_playback;

gcc_const
static SongTime
ToSongTime(mad_timer_t t)
{
	return SongTime::FromMS(mad_timer_count(t, MAD_UNITS_MILLISECONDS));
}

static inline int32_t
mad_fixed_to_24_sample(mad_fixed_t sample)
{
	static constexpr unsigned bits = 24;
	static constexpr mad_fixed_t MIN = -MAD_F_ONE;
	static constexpr mad_fixed_t MAX = MAD_F_ONE - 1;

	/* round */
	sample = sample + (1L << (MAD_F_FRACBITS - bits));

	/* clip */
	if (gcc_unlikely(sample > MAX))
		sample = MAX;
	else if (gcc_unlikely(sample < MIN))
		sample = MIN;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - bits);
}

static void
mad_fixed_to_24_buffer(int32_t *dest, const struct mad_synth *synth,
		       unsigned int start, unsigned int end,
		       unsigned int num_channels)
{
	for (unsigned i = start; i < end; ++i)
		for (unsigned c = 0; c < num_channels; ++c)
			*dest++ = mad_fixed_to_24_sample(synth->pcm.samples[c][i]);
}

static bool
mp3_plugin_init(gcc_unused const config_param &param)
{
	gapless_playback = config_get_bool(CONF_GAPLESS_MP3_PLAYBACK,
					   DEFAULT_GAPLESS_MP3_PLAYBACK);
	return true;
}

struct MadDecoder {
	static constexpr size_t READ_BUFFER_SIZE = 40960;
	static constexpr size_t MP3_DATA_OUTPUT_BUFFER_SIZE = 2048;

	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
	mad_timer_t timer;
	unsigned char input_buffer[READ_BUFFER_SIZE];
	int32_t output_buffer[MP3_DATA_OUTPUT_BUFFER_SIZE];
	SignedSongTime total_time;
	SongTime elapsed_time;
	SongTime seek_time;
	enum muteframe mute_frame;
	long *frame_offsets;
	mad_timer_t *times;
	unsigned long highest_frame;
	unsigned long max_frames;
	unsigned long current_frame;
	unsigned int drop_start_frames;
	unsigned int drop_end_frames;
	unsigned int drop_start_samples;
	unsigned int drop_end_samples;
	bool found_replay_gain;
	bool found_first_frame;
	bool decoded_first_frame;
	unsigned long bit_rate;
	Decoder *const decoder;
	InputStream &input_stream;
	enum mad_layer layer;

	MadDecoder(Decoder *decoder, InputStream &input_stream);
	~MadDecoder();

	bool Seek(long offset);
	bool FillBuffer();
	void ParseId3(size_t tagsize, Tag **mpd_tag);
	enum mp3_action DecodeNextFrameHeader(Tag **tag);
	enum mp3_action DecodeNextFrame();

	gcc_pure
	offset_type ThisFrameOffset() const;

	gcc_pure
	offset_type RestIncludingThisFrame() const;

	/**
	 * Attempt to calulcate the length of the song from filesize
	 */
	void FileSizeToSongLength();

	bool DecodeFirstFrame(Tag **tag);

	gcc_pure
	long TimeToFrame(SongTime t) const;

	void UpdateTimerNextFrame();

	/**
	 * Sends the synthesized current frame via decoder_data().
	 */
	DecoderCommand SendPCM(unsigned i, unsigned pcm_length);

	/**
	 * Synthesize the current frame and send it via
	 * decoder_data().
	 */
	DecoderCommand SyncAndSend();

	bool Read();
};

MadDecoder::MadDecoder(Decoder *_decoder,
		       InputStream &_input_stream)
	:mute_frame(MUTEFRAME_NONE),
	 frame_offsets(nullptr),
	 times(nullptr),
	 highest_frame(0), max_frames(0), current_frame(0),
	 drop_start_frames(0), drop_end_frames(0),
	 drop_start_samples(0), drop_end_samples(0),
	 found_replay_gain(false),
	 found_first_frame(false), decoded_first_frame(false),
	 decoder(_decoder), input_stream(_input_stream),
	 layer(mad_layer(0))
{
	mad_stream_init(&stream);
	mad_stream_options(&stream, MAD_OPTION_IGNORECRC);
	mad_frame_init(&frame);
	mad_synth_init(&synth);
	mad_timer_reset(&timer);
}

inline bool
MadDecoder::Seek(long offset)
{
	Error error;
	if (!input_stream.LockSeek(offset, error))
		return false;

	mad_stream_buffer(&stream, input_buffer, 0);
	stream.error = MAD_ERROR_NONE;

	return true;
}

inline bool
MadDecoder::FillBuffer()
{
	size_t remaining, length;
	unsigned char *dest;

	if (stream.next_frame != nullptr) {
		remaining = stream.bufend - stream.next_frame;
		memmove(input_buffer, stream.next_frame, remaining);
		dest = input_buffer + remaining;
		length = READ_BUFFER_SIZE - remaining;
	} else {
		remaining = 0;
		length = READ_BUFFER_SIZE;
		dest = input_buffer;
	}

	/* we've exhausted the read buffer, so give up!, these potential
	 * mp3 frames are way too big, and thus unlikely to be mp3 frames */
	if (length == 0)
		return false;

	length = decoder_read(decoder, input_stream, dest, length);
	if (length == 0)
		return false;

	mad_stream_buffer(&stream, input_buffer, length + remaining);
	stream.error = MAD_ERROR_NONE;

	return true;
}

#ifdef HAVE_ID3TAG
static bool
parse_id3_replay_gain_info(ReplayGainInfo &rgi,
			   struct id3_tag *tag)
{
	bool found = false;

	rgi.Clear();

	struct id3_frame *frame;
	for (unsigned i = 0; (frame = id3_tag_findframe(tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		char *const key = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[1]));
		char *const value = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[2]));

		if (ParseReplayGainTag(rgi, key, value))
			found = true;

		free(key);
		free(value);
	}

	return found ||
		/* fall back on RVA2 if no replaygain tags found */
		tag_rva2_parse(tag, rgi);
}
#endif

#ifdef HAVE_ID3TAG
gcc_pure
static MixRampInfo
parse_id3_mixramp(struct id3_tag *tag)
{
	MixRampInfo result;

	struct id3_frame *frame;
	for (unsigned i = 0; (frame = id3_tag_findframe(tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		char *const key = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[1]));
		char *const value = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[2]));

		ParseMixRampTag(result, key, value);

		free(key);
		free(value);
	}

	return result;
}
#endif

inline void
MadDecoder::ParseId3(size_t tagsize, Tag **mpd_tag)
{
#ifdef HAVE_ID3TAG
	id3_byte_t *allocated = nullptr;

	const id3_length_t count = stream.bufend - stream.this_frame;

	const id3_byte_t *id3_data;
	if (tagsize <= count) {
		id3_data = stream.this_frame;
		mad_stream_skip(&(stream), tagsize);
	} else {
		allocated = new id3_byte_t[tagsize];
		memcpy(allocated, stream.this_frame, count);
		mad_stream_skip(&(stream), count);

		if (!decoder_read_full(decoder, input_stream,
				       allocated + count, tagsize - count)) {
			LogDebug(mad_domain, "error parsing ID3 tag");
			delete[] allocated;
			return;
		}

		id3_data = allocated;
	}

	struct id3_tag *const id3_tag = id3_tag_parse(id3_data, tagsize);
	if (id3_tag == nullptr) {
		delete[] allocated;
		return;
	}

	if (mpd_tag) {
		Tag *tmp_tag = tag_id3_import(id3_tag);
		if (tmp_tag != nullptr) {
			delete *mpd_tag;
			*mpd_tag = tmp_tag;
		}
	}

	if (decoder != nullptr) {
		ReplayGainInfo rgi;

		if (parse_id3_replay_gain_info(rgi, id3_tag)) {
			decoder_replay_gain(*decoder, &rgi);
			found_replay_gain = true;
		}

		decoder_mixramp(*decoder, parse_id3_mixramp(id3_tag));
	}

	id3_tag_delete(id3_tag);

	delete[] allocated;
#else /* !HAVE_ID3TAG */
	(void)mpd_tag;

	/* This code is enabled when libid3tag is disabled.  Instead
	   of parsing the ID3 frame, it just skips it. */

	size_t count = stream.bufend - stream.this_frame;

	if (tagsize <= count) {
		mad_stream_skip(&stream, tagsize);
	} else {
		mad_stream_skip(&stream, count);
		decoder_skip(decoder, input_stream, tagsize - count);
	}
#endif
}

#ifndef HAVE_ID3TAG
/**
 * This function emulates libid3tag when it is disabled.  Instead of
 * doing a real analyzation of the frame, it just checks whether the
 * frame begins with the string "ID3".  If so, it returns the length
 * of the ID3 frame.
 */
static signed long
id3_tag_query(const void *p0, size_t length)
{
	const char *p = (const char *)p0;

	return length >= 10 && memcmp(p, "ID3", 3) == 0
		? (p[8] << 7) + p[9] + 10
		: 0;
}
#endif /* !HAVE_ID3TAG */

static enum mp3_action
RecoverFrameError(struct mad_stream &stream)
{
	if (MAD_RECOVERABLE(stream.error))
		return DECODE_SKIP;
	else if (stream.error == MAD_ERROR_BUFLEN)
		return DECODE_CONT;

	FormatWarning(mad_domain,
		      "unrecoverable frame level error: %s",
		      mad_stream_errorstr(&stream));
	return DECODE_BREAK;
}

enum mp3_action
MadDecoder::DecodeNextFrameHeader(Tag **tag)
{
	if ((stream.buffer == nullptr || stream.error == MAD_ERROR_BUFLEN) &&
	    !FillBuffer())
		return DECODE_BREAK;

	if (mad_header_decode(&frame.header, &stream)) {
		if (stream.error == MAD_ERROR_LOSTSYNC && stream.this_frame) {
			signed long tagsize = id3_tag_query(stream.this_frame,
							    stream.bufend -
							    stream.this_frame);

			if (tagsize > 0) {
				if (tag && !(*tag)) {
					ParseId3((size_t)tagsize, tag);
				} else {
					mad_stream_skip(&stream, tagsize);
				}
				return DECODE_CONT;
			}
		}

		return RecoverFrameError(stream);
	}

	enum mad_layer new_layer = frame.header.layer;
	if (layer == (mad_layer)0) {
		if (new_layer != MAD_LAYER_II && new_layer != MAD_LAYER_III) {
			/* Only layer 2 and 3 have been tested to work */
			return DECODE_SKIP;
		}

		layer = new_layer;
	} else if (new_layer != layer) {
		/* Don't decode frames with a different layer than the first */
		return DECODE_SKIP;
	}

	return DECODE_OK;
}

enum mp3_action
MadDecoder::DecodeNextFrame()
{
	if ((stream.buffer == nullptr || stream.error == MAD_ERROR_BUFLEN) &&
	    !FillBuffer())
		return DECODE_BREAK;

	if (mad_frame_decode(&frame, &stream)) {
		if (stream.error == MAD_ERROR_LOSTSYNC) {
			signed long tagsize = id3_tag_query(stream.this_frame,
							    stream.bufend -
							    stream.this_frame);
			if (tagsize > 0) {
				mad_stream_skip(&stream, tagsize);
				return DECODE_CONT;
			}
		}

		return RecoverFrameError(stream);
	}

	return DECODE_OK;
}

/* xing stuff stolen from alsaplayer, and heavily modified by jat */
static constexpr unsigned XI_MAGIC = (('X' << 8) | 'i');
static constexpr unsigned NG_MAGIC = (('n' << 8) | 'g');
static constexpr unsigned IN_MAGIC = (('I' << 8) | 'n');
static constexpr unsigned FO_MAGIC = (('f' << 8) | 'o');

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

static const unsigned XING_FRAMES = 1;
static const unsigned XING_BYTES = 2;
static const unsigned XING_TOC = 4;
static const unsigned XING_SCALE = 8;

struct lame_version {
	unsigned major;
	unsigned minor;
};

struct lame {
	char encoder[10];       /* 9 byte encoder name/version ("LAME3.97b") */
	struct lame_version version; /* struct containing just the version */
	float peak;             /* replaygain peak */
	float track_gain;       /* replaygain track gain */
	float album_gain;       /* replaygain album gain */
	int encoder_delay;      /* # of added samples at start of mp3 */
	int encoder_padding;    /* # of added samples at end of mp3 */
	int crc;                /* CRC of the first 190 bytes of this frame */
};

static bool
parse_xing(struct xing *xing, struct mad_bitptr *ptr, int *oldbitlen)
{
	int bitlen = *oldbitlen;

	if (bitlen < 16)
		return false;

	const unsigned long bits = mad_bit_read(ptr, 16);
	bitlen -= 16;

	if (bits == XI_MAGIC) {
		if (bitlen < 16)
			return false;

		if (mad_bit_read(ptr, 16) != NG_MAGIC)
			return false;

		bitlen -= 16;
		xing->magic = XING_MAGIC_XING;
	} else if (bits == IN_MAGIC) {
		if (bitlen < 16)
			return false;

		if (mad_bit_read(ptr, 16) != FO_MAGIC)
			return false;

		bitlen -= 16;
		xing->magic = XING_MAGIC_INFO;
	}
	else if (bits == NG_MAGIC) xing->magic = XING_MAGIC_XING;
	else if (bits == FO_MAGIC) xing->magic = XING_MAGIC_INFO;
	else
		return false;

	if (bitlen < 32)
		return false;
	xing->flags = mad_bit_read(ptr, 32);
	bitlen -= 32;

	if (xing->flags & XING_FRAMES) {
		if (bitlen < 32)
			return false;
		xing->frames = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	if (xing->flags & XING_BYTES) {
		if (bitlen < 32)
			return false;
		xing->bytes = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	if (xing->flags & XING_TOC) {
		if (bitlen < 800)
			return false;
		for (unsigned i = 0; i < 100; ++i)
			xing->toc[i] = mad_bit_read(ptr, 8);
		bitlen -= 800;
	}

	if (xing->flags & XING_SCALE) {
		if (bitlen < 32)
			return false;
		xing->scale = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	/* Make sure we consume no less than 120 bytes (960 bits) in hopes that
	 * the LAME tag is found there, and not right after the Xing header */
	const int bitsleft = 960 - (*oldbitlen - bitlen);
	if (bitsleft < 0)
		return false;
	else if (bitsleft > 0) {
		mad_bit_read(ptr, bitsleft);
		bitlen -= bitsleft;
	}

	*oldbitlen = bitlen;

	return true;
}

static bool
parse_lame(struct lame *lame, struct mad_bitptr *ptr, int *bitlen)
{
	/* Unlike the xing header, the lame tag has a fixed length.  Fail if
	 * not all 36 bytes (288 bits) are there. */
	if (*bitlen < 288)
		return false;

	for (unsigned i = 0; i < 9; i++)
		lame->encoder[i] = (char)mad_bit_read(ptr, 8);
	lame->encoder[9] = '\0';

	*bitlen -= 72;

	/* This is technically incorrect, since the encoder might not be lame.
	 * But there's no other way to determine if this is a lame tag, and we
	 * wouldn't want to go reading a tag that's not there. */
	if (!StringStartsWith(lame->encoder, "LAME"))
		return false;

	if (sscanf(lame->encoder+4, "%u.%u",
	           &lame->version.major, &lame->version.minor) != 2)
		return false;

	FormatDebug(mad_domain, "detected LAME version %i.%i (\"%s\")",
		    lame->version.major, lame->version.minor, lame->encoder);

	/* The reference volume was changed from the 83dB used in the
	 * ReplayGain spec to 89dB in lame 3.95.1.  Bump the gain for older
	 * versions, since everyone else uses 89dB instead of 83dB.
	 * Unfortunately, lame didn't differentiate between 3.95 and 3.95.1, so
	 * it's impossible to make the proper adjustment for 3.95.
	 * Fortunately, 3.95 was only out for about a day before 3.95.1 was
	 * released. -- tmz */
	int adj = 0;
	if (lame->version.major < 3 ||
	    (lame->version.major == 3 && lame->version.minor < 95))
		adj = 6;

	mad_bit_read(ptr, 16);

	lame->peak = mad_f_todouble(mad_bit_read(ptr, 32) << 5); /* peak */
	FormatDebug(mad_domain, "LAME peak found: %f", lame->peak);

	lame->track_gain = 0;
	unsigned name = mad_bit_read(ptr, 3); /* gain name */
	unsigned orig = mad_bit_read(ptr, 3); /* gain originator */
	unsigned sign = mad_bit_read(ptr, 1); /* sign bit */
	int gain = mad_bit_read(ptr, 9); /* gain*10 */
	if (gain && name == 1 && orig != 0) {
		lame->track_gain = ((sign ? -gain : gain) / 10.0) + adj;
		FormatDebug(mad_domain, "LAME track gain found: %f",
			    lame->track_gain);
	}

	/* tmz reports that this isn't currently written by any version of lame
	 * (as of 3.97).  Since we have no way of testing it, don't use it.
	 * Wouldn't want to go blowing someone's ears just because we read it
	 * wrong. :P -- jat */
	lame->album_gain = 0;
#if 0
	name = mad_bit_read(ptr, 3); /* gain name */
	orig = mad_bit_read(ptr, 3); /* gain originator */
	sign = mad_bit_read(ptr, 1); /* sign bit */
	gain = mad_bit_read(ptr, 9); /* gain*10 */
	if (gain && name == 2 && orig != 0) {
		lame->album_gain = ((sign ? -gain : gain) / 10.0) + adj;
		FormatDebug(mad_domain, "LAME album gain found: %f",
			    lame->track_gain);
	}
#else
	mad_bit_read(ptr, 16);
#endif

	mad_bit_read(ptr, 16);

	lame->encoder_delay = mad_bit_read(ptr, 12);
	lame->encoder_padding = mad_bit_read(ptr, 12);

	FormatDebug(mad_domain, "encoder delay is %i, encoder padding is %i",
		    lame->encoder_delay, lame->encoder_padding);

	mad_bit_read(ptr, 80);

	lame->crc = mad_bit_read(ptr, 16);

	*bitlen -= 216;

	return true;
}

static inline SongTime
mp3_frame_duration(const struct mad_frame *frame)
{
	return ToSongTime(frame->header.duration);
}

inline offset_type
MadDecoder::ThisFrameOffset() const
{
	auto offset = input_stream.GetOffset();

	if (stream.this_frame != nullptr)
		offset -= stream.bufend - stream.this_frame;
	else
		offset -= stream.bufend - stream.buffer;

	return offset;
}

inline offset_type
MadDecoder::RestIncludingThisFrame() const
{
	return input_stream.GetSize() - ThisFrameOffset();
}

inline void
MadDecoder::FileSizeToSongLength()
{
	if (input_stream.KnownSize()) {
		offset_type rest = RestIncludingThisFrame();

		const SongTime frame_duration = mp3_frame_duration(&frame);
		const SongTime duration =
			SongTime::FromScale<uint64_t>(rest,
						      frame.header.bitrate / 8);
		total_time = duration;

		max_frames = (frame_duration.IsPositive()
			      ? duration.count() / frame_duration.count()
			      : 0)
			+ FRAMES_CUSHION;
	} else {
		max_frames = FRAMES_CUSHION;
		total_time = SignedSongTime::Negative();
	}
}

inline bool
MadDecoder::DecodeFirstFrame(Tag **tag)
{
	struct xing xing;
	xing.frames = 0;

	while (true) {
		enum mp3_action ret;
		do {
			ret = DecodeNextFrameHeader(tag);
		} while (ret == DECODE_CONT);
		if (ret == DECODE_BREAK)
			return false;
		if (ret == DECODE_SKIP) continue;

		do {
			ret = DecodeNextFrame();
		} while (ret == DECODE_CONT);
		if (ret == DECODE_BREAK)
			return false;
		if (ret == DECODE_OK) break;
	}

	struct mad_bitptr ptr = stream.anc_ptr;
	int bitlen = stream.anc_bitlen;

	FileSizeToSongLength();

	/*
	 * if an xing tag exists, use that!
	 */
	if (parse_xing(&xing, &ptr, &bitlen)) {
		mute_frame = MUTEFRAME_SKIP;

		if ((xing.flags & XING_FRAMES) && xing.frames) {
			mad_timer_t duration = frame.header.duration;
			mad_timer_multiply(&duration, xing.frames);
			total_time = ToSongTime(duration);
			max_frames = xing.frames;
		}

		struct lame lame;
		if (parse_lame(&lame, &ptr, &bitlen)) {
			if (gapless_playback && input_stream.IsSeekable()) {
				drop_start_samples = lame.encoder_delay +
				                           DECODERDELAY;
				drop_end_samples = lame.encoder_padding;
			}

			/* Album gain isn't currently used.  See comment in
			 * parse_lame() for details. -- jat */
			if (decoder != nullptr && !found_replay_gain &&
			    lame.track_gain) {
				ReplayGainInfo rgi;
				rgi.Clear();
				rgi.tuples[REPLAY_GAIN_TRACK].gain = lame.track_gain;
				rgi.tuples[REPLAY_GAIN_TRACK].peak = lame.peak;
				decoder_replay_gain(*decoder, &rgi);
			}
		}
	}

	if (!max_frames)
		return false;

	if (max_frames > 8 * 1024 * 1024) {
		FormatWarning(mad_domain,
			      "mp3 file header indicates too many frames: %lu",
			      max_frames);
		return false;
	}

	frame_offsets = new long[max_frames];
	times = new mad_timer_t[max_frames];

	return true;
}

MadDecoder::~MadDecoder()
{
	mad_synth_finish(&synth);
	mad_frame_finish(&frame);
	mad_stream_finish(&stream);

	delete[] frame_offsets;
	delete[] times;
}

/* this is primarily used for getting total time for tags */
static std::pair<bool, SignedSongTime>
mad_decoder_total_file_time(InputStream &is)
{
	MadDecoder data(nullptr, is);
	return data.DecodeFirstFrame(nullptr)
		? std::make_pair(true, data.total_time)
		: std::make_pair(false, SignedSongTime::Negative());
}

long
MadDecoder::TimeToFrame(SongTime t) const
{
	unsigned long i;

	for (i = 0; i < highest_frame; ++i) {
		auto frame_time = ToSongTime(times[i]);
		if (frame_time >= t)
			break;
	}

	return i;
}

void
MadDecoder::UpdateTimerNextFrame()
{
	if (current_frame >= highest_frame) {
		/* record this frame's properties in frame_offsets
		   (for seeking) and times */
		bit_rate = frame.header.bitrate;

		if (current_frame >= max_frames)
			/* cap current_frame */
			current_frame = max_frames - 1;
		else
			highest_frame++;

		frame_offsets[current_frame] = ThisFrameOffset();

		mad_timer_add(&timer, frame.header.duration);
		times[current_frame] = timer;
	} else
		/* get the new timer value from "times" */
		timer = times[current_frame];

	current_frame++;
	elapsed_time = ToSongTime(timer);
}

DecoderCommand
MadDecoder::SendPCM(unsigned i, unsigned pcm_length)
{
	unsigned max_samples = sizeof(output_buffer) /
		sizeof(output_buffer[0]) /
		MAD_NCHANNELS(&frame.header);

	while (i < pcm_length) {
		unsigned int num_samples = pcm_length - i;
		if (num_samples > max_samples)
			num_samples = max_samples;

		i += num_samples;

		mad_fixed_to_24_buffer(output_buffer, &synth,
				       i - num_samples, i,
				       MAD_NCHANNELS(&frame.header));
		num_samples *= MAD_NCHANNELS(&frame.header);

		auto cmd = decoder_data(*decoder, input_stream, output_buffer,
					sizeof(output_buffer[0]) * num_samples,
					bit_rate / 1000);
		if (cmd != DecoderCommand::NONE)
			return cmd;
	}

	return DecoderCommand::NONE;
}

inline DecoderCommand
MadDecoder::SyncAndSend()
{
	mad_synth_frame(&synth, &frame);

	if (!found_first_frame) {
		unsigned int samples_per_frame = synth.pcm.length;
		drop_start_frames = drop_start_samples / samples_per_frame;
		drop_end_frames = drop_end_samples / samples_per_frame;
		drop_start_samples = drop_start_samples % samples_per_frame;
		drop_end_samples = drop_end_samples % samples_per_frame;
		found_first_frame = true;
	}

	if (drop_start_frames > 0) {
		drop_start_frames--;
		return DecoderCommand::NONE;
	} else if ((drop_end_frames > 0) &&
		   (current_frame == (max_frames + 1 - drop_end_frames))) {
		/* stop decoding, effectively dropping all remaining
		   frames */
		return DecoderCommand::STOP;
	}

	unsigned i = 0;
	if (!decoded_first_frame) {
		i = drop_start_samples;
		decoded_first_frame = true;
	}

	unsigned pcm_length = synth.pcm.length;
	if (drop_end_samples &&
	    (current_frame == max_frames - drop_end_frames)) {
		if (drop_end_samples >= pcm_length)
			pcm_length = 0;
		else
			pcm_length -= drop_end_samples;
	}

	auto cmd = SendPCM(i, pcm_length);
	if (cmd != DecoderCommand::NONE)
		return cmd;

	if (drop_end_samples &&
	    (current_frame == max_frames - drop_end_frames))
		/* stop decoding, effectively dropping
		 * all remaining samples */
		return DecoderCommand::STOP;

	return DecoderCommand::NONE;
}

inline bool
MadDecoder::Read()
{
	UpdateTimerNextFrame();

	switch (mute_frame) {
		DecoderCommand cmd;

	case MUTEFRAME_SKIP:
		mute_frame = MUTEFRAME_NONE;
		break;
	case MUTEFRAME_SEEK:
		if (elapsed_time >= seek_time)
			mute_frame = MUTEFRAME_NONE;
		break;
	case MUTEFRAME_NONE:
		cmd = SyncAndSend();
		if (cmd == DecoderCommand::SEEK) {
			assert(input_stream.IsSeekable());

			unsigned long j =
				TimeToFrame(decoder_seek_time(*decoder));
			if (j < highest_frame) {
				if (Seek(frame_offsets[j])) {
					current_frame = j;
					decoder_command_finished(*decoder);
				} else
					decoder_seek_error(*decoder);
			} else {
				seek_time = decoder_seek_time(*decoder);
				mute_frame = MUTEFRAME_SEEK;
				decoder_command_finished(*decoder);
			}
		} else if (cmd != DecoderCommand::NONE)
			return false;
	}

	while (true) {
		enum mp3_action ret;
		do {
			Tag *tag = nullptr;

			ret = DecodeNextFrameHeader(&tag);

			if (tag != nullptr) {
				decoder_tag(*decoder, input_stream,
					    std::move(*tag));
				delete tag;
			}
		} while (ret == DECODE_CONT);
		if (ret == DECODE_BREAK)
			return false;

		const bool skip = ret == DECODE_SKIP;

		if (mute_frame == MUTEFRAME_NONE) {
			do {
				ret = DecodeNextFrame();
			} while (ret == DECODE_CONT);
			if (ret == DECODE_BREAK)
				return false;
		}

		if (!skip && ret == DECODE_OK)
			return true;
	}
}

static void
mp3_decode(Decoder &decoder, InputStream &input_stream)
{
	MadDecoder data(&decoder, input_stream);

	Tag *tag = nullptr;
	if (!data.DecodeFirstFrame(&tag)) {
		delete tag;

		if (decoder_get_command(decoder) == DecoderCommand::NONE)
			LogError(mad_domain,
				 "input/Input does not appear to be a mp3 bit stream");
		return;
	}

	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format,
				       data.frame.header.samplerate,
				       SampleFormat::S24_P32,
				       MAD_NCHANNELS(&data.frame.header),
				       error)) {
		LogError(error);
		delete tag;
		return;
	}

	decoder_initialized(decoder, audio_format,
			    input_stream.IsSeekable(),
			    data.total_time);

	if (tag != nullptr) {
		decoder_tag(decoder, input_stream, std::move(*tag));
		delete tag;
	}

	while (data.Read()) {}
}

static bool
mad_decoder_scan_stream(InputStream &is,
			const struct tag_handler *handler, void *handler_ctx)
{
	const auto result = mad_decoder_total_file_time(is);
	if (!result.first)
		return false;

	if (!result.second.IsNegative())
		tag_handler_invoke_duration(handler, handler_ctx,
					    SongTime(result.second));
	return true;
}

static const char *const mp3_suffixes[] = { "mp3", "mp2", nullptr };
static const char *const mp3_mime_types[] = { "audio/mpeg", nullptr };

const struct DecoderPlugin mad_decoder_plugin = {
	"mad",
	mp3_plugin_init,
	nullptr,
	mp3_decode,
	nullptr,
	nullptr,
	mad_decoder_scan_stream,
	nullptr,
	mp3_suffixes,
	mp3_mime_types,
};
