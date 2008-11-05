/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * WavPack support added by Laszlo Ashin <kodest@gmail.com>
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
#include "../path.h"

#include <wavpack/wavpack.h>
#include <glib.h>

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

#define ERRORLEN 80

static struct {
	const char *name;
	enum tag_type type;
} tagtypes[] = {
	{ "artist", TAG_ITEM_ARTIST },
	{ "album", TAG_ITEM_ALBUM },
	{ "title", TAG_ITEM_TITLE },
	{ "track", TAG_ITEM_TRACK },
	{ "name", TAG_ITEM_NAME },
	{ "genre", TAG_ITEM_GENRE },
	{ "date", TAG_ITEM_DATE },
	{ "composer", TAG_ITEM_COMPOSER },
	{ "performer", TAG_ITEM_PERFORMER },
	{ "comment", TAG_ITEM_COMMENT },
	{ "disc", TAG_ITEM_DISC },
};

/*
 * This function has been borrowed from the tiny player found on
 * wavpack.com. Modifications were required because mpd only handles
 * max 16 bit samples.
 */
static void
format_samples_int(int bytes_per_sample, void *buffer, uint32_t samcnt)
{
	int32_t temp;
	uchar *dst = (uchar *)buffer;
	int32_t *src = (int32_t *)buffer;

	switch (bytes_per_sample) {
	case 1:
		while (samcnt--)
			*dst++ = *src++;
		break;
	case 2:
		while (samcnt--) {
			temp = *src++;
#ifdef WORDS_BIGENDIAN
			*dst++ = (uchar)(temp >> 8);
			*dst++ = (uchar)(temp);
#else
			*dst++ = (uchar)(temp);
			*dst++ = (uchar)(temp >> 8);
#endif
		}
		break;
	case 3:
		/* downscale to 16 bits */
		while (samcnt--) {
			temp = *src++;
#ifdef WORDS_BIGENDIAN
			*dst++ = (uchar)(temp >> 16);
			*dst++ = (uchar)(temp >> 8);
#else
			*dst++ = (uchar)(temp >> 8);
			*dst++ = (uchar)(temp >> 16);
#endif
		}
		break;
	case 4:
		/* downscale to 16 bits */
		while (samcnt--) {
			temp = *src++;
#ifdef WORDS_BIGENDIAN
			*dst++ = (uchar)(temp >> 24);
			*dst++ = (uchar)(temp >> 16);

#else
			*dst++ = (uchar)(temp >> 16);
			*dst++ = (uchar)(temp >> 24);
#endif
		}
		break;
	}
}

/*
 * This function converts floating point sample data to 16 bit integer.
 */
static void format_samples_float(mpd_unused int bytes_per_sample, void *buffer,
				 uint32_t samcnt)
{
	int16_t *dst = (int16_t *)buffer;
	float *src = (float *)buffer;

	while (samcnt--) {
		*dst++ = (int16_t)(*src++);
	}
}

/*
 * This does the main decoding thing.
 * Requires an already opened WavpackContext.
 */
static void wavpack_decode(struct decoder * decoder,
                           WavpackContext *wpc, bool canseek,
                           ReplayGainInfo *replayGainInfo)
{
	struct audio_format audio_format;
	void (*format_samples)(int bytes_per_sample,
			       void *buffer, uint32_t samcnt);
	char chunk[CHUNK_SIZE];
	float file_time;
	int samplesreq, samplesgot;
	int allsamples;
	int position, outsamplesize;
	int bytes_per_sample;

	audio_format.sample_rate = WavpackGetSampleRate(wpc);
	audio_format.channels = WavpackGetReducedChannels(wpc);
	audio_format.bits = WavpackGetBitsPerSample(wpc);

	if (audio_format.bits > 16)
		audio_format.bits = 16;

	if ((WavpackGetMode(wpc) & MODE_FLOAT) == MODE_FLOAT)
		format_samples = format_samples_float;
	else
		format_samples = format_samples_int;
/*
	if ((WavpackGetMode(wpc) & MODE_WVC) == MODE_WVC)
		ERROR("decoding WITH wvc!!!\n");
	else
		ERROR("decoding without wvc\n");
*/
	allsamples = WavpackGetNumSamples(wpc);
	bytes_per_sample = WavpackGetBytesPerSample(wpc);

	outsamplesize = bytes_per_sample;
	if (outsamplesize > 2)
		outsamplesize = 2;
	outsamplesize *= audio_format.channels;

	samplesreq = sizeof(chunk) / (4 * audio_format.channels);

	decoder_initialized(decoder, &audio_format, false,
			    (float)allsamples / audio_format.sample_rate);

	position = 0;

	do {
		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK) {
			if (canseek) {
				int where;

				where = decoder_seek_where(decoder) *
					audio_format.sample_rate;
				if (WavpackSeekSample(wpc, where)) {
					position = where;
					decoder_command_finished(decoder);
				} else
					decoder_seek_error(decoder);
			} else {
				decoder_seek_error(decoder);
			}
		}

		if (decoder_get_command(decoder) == DECODE_COMMAND_STOP)
			break;

		samplesgot = WavpackUnpackSamples(wpc,
		                                  (int32_t *)chunk, samplesreq);
		if (samplesgot > 0) {
			int bitrate = (int)(WavpackGetInstantBitrate(wpc) /
			              1000 + 0.5);
			position += samplesgot;
			file_time = (float)position / audio_format.sample_rate;

			format_samples(bytes_per_sample, chunk,
			               samplesgot * audio_format.channels);

			decoder_data(decoder, NULL, chunk,
				     samplesgot * outsamplesize,
				     file_time, bitrate,
				     replayGainInfo);
		}
	} while (samplesgot == samplesreq);
}

static char *wavpack_tag(WavpackContext *wpc, char *key)
{
	char *value = NULL;
	int size;

	size = WavpackGetTagItem(wpc, key, NULL, 0);
	if (size > 0) {
		size++;
		value = g_malloc(size);
		WavpackGetTagItem(wpc, key, value, size);
	}

	return value;
}

static ReplayGainInfo *wavpack_replaygain(WavpackContext *wpc)
{
	static char replaygain_track_gain[] = "replaygain_track_gain";
	static char replaygain_album_gain[] = "replaygain_album_gain";
	static char replaygain_track_peak[] = "replaygain_track_peak";
	static char replaygain_album_peak[] = "replaygain_album_peak";
	ReplayGainInfo *replay_gain_info;
	bool found = false;
	char *value;

	replay_gain_info = newReplayGainInfo();

	value = wavpack_tag(wpc, replaygain_track_gain);
	if (value) {
		replay_gain_info->trackGain = atof(value);
		free(value);
		found = true;
	}

	value = wavpack_tag(wpc, replaygain_album_gain);
	if (value) {
		replay_gain_info->albumGain = atof(value);
		free(value);
		found = true;
	}

	value = wavpack_tag(wpc, replaygain_track_peak);
	if (value) {
		replay_gain_info->trackPeak = atof(value);
		free(value);
		found = true;
	}

	value = wavpack_tag(wpc, replaygain_album_peak);
	if (value) {
		replay_gain_info->albumPeak = atof(value);
		free(value);
		found = true;
	}


	if (found)
		return replay_gain_info;

	freeReplayGainInfo(replay_gain_info);

	return NULL;
}

/*
 * Reads metainfo from the specified file.
 */
static struct tag *wavpack_tagdup(const char *fname)
{
	WavpackContext *wpc;
	struct tag *tag;
	char error[ERRORLEN];
	char *s;
	int ssize;
	int j;

	wpc = WavpackOpenFileInput(fname, error, OPEN_TAGS, 0);
	if (wpc == NULL) {
		g_warning("failed to open WavPack file \"%s\": %s\n",
			  fname, error);
		return NULL;
	}

	tag = tag_new();
	tag->time =
		(float)WavpackGetNumSamples(wpc) / WavpackGetSampleRate(wpc);

	ssize = 0;
	s = NULL;

	for (unsigned i = 0; i < G_N_ELEMENTS(tagtypes); ++i) {
		j = WavpackGetTagItem(wpc, tagtypes[i].name, NULL, 0);
		if (j > 0) {
			++j;

			if (s == NULL) {
				s = g_malloc(j);
				ssize = j;
			} else if (j > ssize) {
				char *t = (char *)g_realloc(s, j);
				ssize = j;
				s = t;
			}

			WavpackGetTagItem(wpc, tagtypes[i].name, s, j);
			tag_add_item(tag, tagtypes[i].type, s);
		}
	}

	g_free(s);

	WavpackCloseFile(wpc);

	return tag;
}

/*
 * mpd input_stream <=> WavpackStreamReader wrapper callbacks
 */

/* This struct is needed for per-stream last_byte storage. */
struct wavpack_input {
	struct decoder *decoder;
	struct input_stream *is;
	/* Needed for push_back_byte() */
	int last_byte;
};

static int32_t read_bytes(void *id, void *data, int32_t bcount)
{
	struct wavpack_input *isp = (struct wavpack_input *)id;
	uint8_t *buf = (uint8_t *)data;
	int32_t i = 0;

	if (isp->last_byte != EOF) {
		*buf++ = isp->last_byte;
		isp->last_byte = EOF;
		--bcount;
		++i;
	}

	/* wavpack fails if we return a partial read, so we just wait
	   until the buffer is full */
	while (bcount > 0) {
		size_t nbytes = decoder_read(isp->decoder, isp->is,
					     buf, bcount);
		if (nbytes == 0)
			/* EOF, error or a decoder command */
			break;

		i += nbytes;
		bcount -= nbytes;
		buf += nbytes;
	}

	return i;
}

static uint32_t get_pos(void *id)
{
	return ((struct wavpack_input *)id)->is->offset;
}

static int set_pos_abs(void *id, uint32_t pos)
{
	return input_stream_seek(((struct wavpack_input *)id)->is, pos, SEEK_SET)
		? 0 : -1;
}

static int set_pos_rel(void *id, int32_t delta, int mode)
{
	return input_stream_seek(((struct wavpack_input *)id)->is, delta, mode)
		? 0 : -1;
}

static int push_back_byte(void *id, int c)
{
	((struct wavpack_input *)id)->last_byte = c;
	return 1;
}

static uint32_t get_length(void *id)
{
	return ((struct wavpack_input *)id)->is->size;
}

static int can_seek(void *id)
{
	return ((struct wavpack_input *)id)->is->seekable;
}

static WavpackStreamReader mpd_is_reader = {
	.read_bytes = read_bytes,
	.get_pos = get_pos,
	.set_pos_abs = set_pos_abs,
	.set_pos_rel = set_pos_rel,
	.push_back_byte = push_back_byte,
	.get_length = get_length,
	.can_seek = can_seek,
	.write_bytes = NULL /* no need to write edited tags */
};

static void
wavpack_input_init(struct wavpack_input *isp, struct decoder *decoder,
		   struct input_stream *is)
{
	isp->decoder = decoder;
	isp->is = is;
	isp->last_byte = EOF;
}

/*
 * Tries to decode the specified stream, and gives true if managed to do it.
 */
static bool wavpack_trydecode(struct input_stream *is)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	struct wavpack_input isp;

	wavpack_input_init(&isp, NULL, is);
	wpc = WavpackOpenFileInputEx(&mpd_is_reader, &isp, NULL, error,
	                             OPEN_STREAMING, 0);
	if (wpc == NULL)
		return false;

	WavpackCloseFile(wpc);

	return true;
}

static bool
wavpack_open_wvc(struct decoder *decoder, struct input_stream *is_wvc,
		 struct wavpack_input *wpi)
{
	char tmp[MPD_PATH_MAX];
	const char *utf8url;
	char *wvc_url = NULL;
	bool ret;
	char first_byte;
	size_t nbytes;

	/*
	 * As we use dc->utf8url, this function will be bad for
	 * single files. utf8url is not absolute file path :/
	 */
	utf8url = decoder_get_url(decoder, tmp);
	if (utf8url == NULL)
		return false;

	wvc_url = g_strconcat(utf8url, "c", NULL);
	ret = input_stream_open(is_wvc, wvc_url);
	g_free(wvc_url);

	if (!ret)
		return false;

	/*
	 * And we try to buffer in order to get know
	 * about a possible 404 error.
	 */
	nbytes = decoder_read(decoder, is_wvc,
			      &first_byte, sizeof(first_byte));
	if (nbytes == 0)
		return false;

	/* push it back */
	wavpack_input_init(wpi, decoder, is_wvc);
	wpi->last_byte = first_byte;
	return true;
}

/*
 * Decodes a stream.
 */
static bool
wavpack_streamdecode(struct decoder * decoder, struct input_stream *is)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	struct input_stream is_wvc;
	int open_flags = OPEN_2CH_MAX | OPEN_NORMALIZE /*| OPEN_STREAMING*/;
	struct wavpack_input isp, isp_wvc;

	if (wavpack_open_wvc(decoder, &is_wvc, &isp_wvc))
		open_flags |= OPEN_WVC;

	wavpack_input_init(&isp, decoder, is);
	wpc = WavpackOpenFileInputEx(&mpd_is_reader, &isp, &isp_wvc, error,
				     open_flags, 15);

	if (wpc == NULL) {
		g_warning("failed to open WavPack stream: %s\n", error);
		return false;
	}

	wavpack_decode(decoder, wpc, is->seekable, NULL);

	WavpackCloseFile(wpc);
	if (open_flags & OPEN_WVC)
		input_stream_close(&is_wvc);

	return true;
}

/*
 * Decodes a file.
 */
static bool
wavpack_filedecode(struct decoder *decoder, const char *fname)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	ReplayGainInfo *replay_gain_info;

	wpc = WavpackOpenFileInput(fname, error,
	                           OPEN_TAGS | OPEN_WVC |
	                           OPEN_2CH_MAX | OPEN_NORMALIZE, 15);
	if (wpc == NULL) {
		g_warning("failed to open WavPack file \"%s\": %s\n",
			  fname, error);
		return false;
	}

	replay_gain_info = wavpack_replaygain(wpc);

	wavpack_decode(decoder, wpc, true, replay_gain_info);

	if (replay_gain_info)
		freeReplayGainInfo(replay_gain_info);

	WavpackCloseFile(wpc);

	return true;
}

static char const *const wavpack_suffixes[] = { "wv", NULL };
static char const *const wavpack_mime_types[] = { "audio/x-wavpack", NULL };

const struct decoder_plugin wavpack_plugin = {
	.name = "wavpack",
	.try_decode = wavpack_trydecode,
	.stream_decode = wavpack_streamdecode,
	.file_decode = wavpack_filedecode,
	.tag_dup = wavpack_tagdup,
	.suffixes = wavpack_suffixes,
	.mime_types = wavpack_mime_types
};
