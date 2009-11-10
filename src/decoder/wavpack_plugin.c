/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "decoder_api.h"
#include "audio_check.h"
#include "path.h"
#include "utils.h"

#include <wavpack/wavpack.h>
#include <glib.h>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "wavpack"

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

#define ERRORLEN 80

static struct {
	const char *name;
	enum tag_type type;
} tagtypes[] = {
	{ "artist", TAG_ARTIST },
	{ "album", TAG_ALBUM },
	{ "title", TAG_TITLE },
	{ "track", TAG_TRACK },
	{ "name", TAG_NAME },
	{ "genre", TAG_GENRE },
	{ "date", TAG_DATE },
	{ "composer", TAG_COMPOSER },
	{ "performer", TAG_PERFORMER },
	{ "comment", TAG_COMMENT },
	{ "disc", TAG_DISC },
};

/** A pointer type for format converter function. */
typedef void (*format_samples_t)(
	int bytes_per_sample,
	void *buffer, uint32_t count
);

/*
 * This function has been borrowed from the tiny player found on
 * wavpack.com. Modifications were required because mpd only handles
 * max 24-bit samples.
 */
static void
format_samples_int(int bytes_per_sample, void *buffer, uint32_t count)
{
	int32_t *src = buffer;

	switch (bytes_per_sample) {
	case 1: {
		uchar *dst = buffer;
		/*
		 * The asserts like the following one are because we do the
		 * formatting of samples within a single buffer. The size
		 * of the output samples never can be greater than the size
		 * of the input ones. Otherwise we would have an overflow.
		 */
		assert_static(sizeof(*dst) <= sizeof(*src));

		/* pass through and align 8-bit samples */
		while (count--) {
			*dst++ = *src++;
		}
		break;
	}
	case 2: {
		uint16_t *dst = buffer;
		assert_static(sizeof(*dst) <= sizeof(*src));

		/* pass through and align 16-bit samples */
		while (count--) {
			*dst++ = *src++;
		}
		break;
	}

	case 3:
	case 4:
		/* do nothing */
		break;
	}
}

/*
 * This function converts floating point sample data to 24-bit integer.
 */
static void
format_samples_float(G_GNUC_UNUSED int bytes_per_sample, void *buffer,
		     uint32_t count)
{
	int32_t *dst = buffer;
	float *src = buffer;
	assert_static(sizeof(*dst) <= sizeof(*src));

	while (count--) {
		*dst++ = (int32_t)(*src++ + 0.5f);
	}
}

/*
 * This does the main decoding thing.
 * Requires an already opened WavpackContext.
 */
static void
wavpack_decode(struct decoder *decoder, WavpackContext *wpc, bool can_seek,
	       struct replay_gain_info *replay_gain_info)
{
	GError *error = NULL;
	unsigned bits;
	struct audio_format audio_format;
	format_samples_t format_samples;
	char chunk[CHUNK_SIZE];
	int samples_requested, samples_got;
	float total_time, current_time;
	int bytes_per_sample, output_sample_size;
	int position;

	bits = WavpackGetBitsPerSample(wpc);

	/* round bitwidth to 8-bit units */
	bits = (bits + 7) & (~7);
	/* MPD handles max 32-bit samples */
	if (bits > 32)
		bits = 32;

	if ((WavpackGetMode(wpc) & MODE_FLOAT) == MODE_FLOAT)
		bits = 24;

	if (!audio_format_init_checked(&audio_format,
				       WavpackGetSampleRate(wpc), bits,
				       WavpackGetNumChannels(wpc), &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	if ((WavpackGetMode(wpc) & MODE_FLOAT) == MODE_FLOAT) {
		format_samples = format_samples_float;
	} else {
		format_samples = format_samples_int;
	}

	total_time = WavpackGetNumSamples(wpc);
	total_time /= audio_format.sample_rate;
	bytes_per_sample = WavpackGetBytesPerSample(wpc);
	output_sample_size = audio_format_frame_size(&audio_format);

	/* wavpack gives us all kind of samples in a 32-bit space */
	samples_requested = sizeof(chunk) / (4 * audio_format.channels);

	decoder_initialized(decoder, &audio_format, can_seek, total_time);

	position = 0;

	do {
		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK) {
			if (can_seek) {
				int where;

				where = decoder_seek_where(decoder);
				where *= audio_format.sample_rate;
				if (WavpackSeekSample(wpc, where)) {
					position = where;
					decoder_command_finished(decoder);
				} else {
					decoder_seek_error(decoder);
				}
			} else {
				decoder_seek_error(decoder);
			}
		}

		if (decoder_get_command(decoder) == DECODE_COMMAND_STOP) {
			break;
		}

		samples_got = WavpackUnpackSamples(
			wpc, (int32_t *)chunk, samples_requested
		);
		if (samples_got > 0) {
			int bitrate = (int)(WavpackGetInstantBitrate(wpc) /
			              1000 + 0.5);
			position += samples_got;
			current_time = position;
			current_time /= audio_format.sample_rate;

			format_samples(
				bytes_per_sample, chunk,
				samples_got * audio_format.channels
			);

			decoder_data(
				decoder, NULL, chunk,
				samples_got * output_sample_size,
				current_time, bitrate,
				replay_gain_info
			);
		}
	} while (samples_got > 0);
}

/**
 * Locate and parse a floating point tag.  Returns true if it was
 * found.
 */
static bool
wavpack_tag_float(WavpackContext *wpc, const char *key, float *value_r)
{
	char buffer[64];
	int ret;

	ret = WavpackGetTagItem(wpc, key, buffer, sizeof(buffer));
	if (ret <= 0)
		return false;

	*value_r = atof(buffer);
	return true;
}

static struct replay_gain_info *
wavpack_replaygain(WavpackContext *wpc)
{
	struct replay_gain_info *replay_gain_info;
	bool found = false;

	replay_gain_info = replay_gain_info_new();

	found |= wavpack_tag_float(
		wpc, "replaygain_track_gain",
		&replay_gain_info->tuples[REPLAY_GAIN_TRACK].gain
	);
	found |= wavpack_tag_float(
		wpc, "replaygain_track_peak",
		&replay_gain_info->tuples[REPLAY_GAIN_TRACK].peak
	);
	found |= wavpack_tag_float(
		wpc, "replaygain_album_gain",
		&replay_gain_info->tuples[REPLAY_GAIN_ALBUM].gain
	);
	found |= wavpack_tag_float(
		wpc, "replaygain_album_peak",
		&replay_gain_info->tuples[REPLAY_GAIN_ALBUM].peak
	);

	if (found) {
		return replay_gain_info;
	}

	replay_gain_info_free(replay_gain_info);

	return NULL;
}

/*
 * Reads metainfo from the specified file.
 */
static struct tag *
wavpack_tagdup(const char *fname)
{
	WavpackContext *wpc;
	struct tag *tag;
	char error[ERRORLEN];
	char *s;
	int size, allocated_size;

	wpc = WavpackOpenFileInput(fname, error, OPEN_TAGS, 0);
	if (wpc == NULL) {
		g_warning(
			"failed to open WavPack file \"%s\": %s\n",
			fname, error
		);
		return NULL;
	}

	tag = tag_new();
	tag->time = WavpackGetNumSamples(wpc);
	tag->time /= WavpackGetSampleRate(wpc);

	allocated_size = 0;
	s = NULL;

	for (unsigned i = 0; i < G_N_ELEMENTS(tagtypes); ++i) {
		size = WavpackGetTagItem(wpc, tagtypes[i].name, NULL, 0);
		if (size > 0) {
			++size; /* EOS */

			if (s == NULL) {
				s = g_malloc(size);
				allocated_size = size;
			} else if (size > allocated_size) {
				char *t = (char *)g_realloc(s, size);
				allocated_size = size;
				s = t;
			}

			WavpackGetTagItem(wpc, tagtypes[i].name, s, size);
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

/**
 * Little wrapper for struct wavpack_input to cast from void *.
 */
static struct wavpack_input *
wpin(void *id)
{
	assert(id);
	return id;
}

static int32_t
wavpack_input_read_bytes(void *id, void *data, int32_t bcount)
{
	uint8_t *buf = (uint8_t *)data;
	int32_t i = 0;

	if (wpin(id)->last_byte != EOF) {
		*buf++ = wpin(id)->last_byte;
		wpin(id)->last_byte = EOF;
		--bcount;
		++i;
	}

	/* wavpack fails if we return a partial read, so we just wait
	   until the buffer is full */
	while (bcount > 0) {
		size_t nbytes = decoder_read(
			wpin(id)->decoder, wpin(id)->is, buf, bcount
		);
		if (nbytes == 0) {
			/* EOF, error or a decoder command */
			break;
		}

		i += nbytes;
		bcount -= nbytes;
		buf += nbytes;
	}

	return i;
}

static uint32_t
wavpack_input_get_pos(void *id)
{
	return wpin(id)->is->offset;
}

static int
wavpack_input_set_pos_abs(void *id, uint32_t pos)
{
	return input_stream_seek(wpin(id)->is, pos, SEEK_SET) ? 0 : -1;
}

static int
wavpack_input_set_pos_rel(void *id, int32_t delta, int mode)
{
	return input_stream_seek(wpin(id)->is, delta, mode) ? 0 : -1;
}

static int
wavpack_input_push_back_byte(void *id, int c)
{
	if (wpin(id)->last_byte == EOF) {
		wpin(id)->last_byte = c;
		return c;
	} else {
		return EOF;
	}
}

static uint32_t
wavpack_input_get_length(void *id)
{
	if (wpin(id)->is->size < 0)
		return 0;

	return wpin(id)->is->size;
}

static int
wavpack_input_can_seek(void *id)
{
	return wpin(id)->is->seekable;
}

static WavpackStreamReader mpd_is_reader = {
	.read_bytes = wavpack_input_read_bytes,
	.get_pos = wavpack_input_get_pos,
	.set_pos_abs = wavpack_input_set_pos_abs,
	.set_pos_rel = wavpack_input_set_pos_rel,
	.push_back_byte = wavpack_input_push_back_byte,
	.get_length = wavpack_input_get_length,
	.can_seek = wavpack_input_can_seek,
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

static bool
wavpack_open_wvc(struct decoder *decoder, struct input_stream *is_wvc,
		 struct wavpack_input *wpi)
{
	char *utf8url;
	char *wvc_url = NULL;
	bool ret;
	char first_byte;
	size_t nbytes;

	/*
	 * As we use dc->utf8url, this function will be bad for
	 * single files. utf8url is not absolute file path :/
	 */
	utf8url = decoder_get_uri(decoder);
	if (utf8url == NULL) {
		return false;
	}

	wvc_url = g_strconcat(utf8url, "c", NULL);
	g_free(utf8url);

	ret = input_stream_open(is_wvc, wvc_url);
	g_free(wvc_url);

	if (!ret) {
		return false;
	}

	/*
	 * And we try to buffer in order to get know
	 * about a possible 404 error.
	 */
	nbytes = decoder_read(
		decoder, is_wvc, &first_byte, sizeof(first_byte)
	);
	if (nbytes == 0) {
		input_stream_close(is_wvc);
		return false;
	}

	/* push it back */
	wavpack_input_init(wpi, decoder, is_wvc);
	wpi->last_byte = first_byte;
	return true;
}

/*
 * Decodes a stream.
 */
static void
wavpack_streamdecode(struct decoder * decoder, struct input_stream *is)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	struct input_stream is_wvc;
	int open_flags = OPEN_NORMALIZE;
	struct wavpack_input isp, isp_wvc;
	bool can_seek = is->seekable;

	if (wavpack_open_wvc(decoder, &is_wvc, &isp_wvc)) {
		open_flags |= OPEN_WVC;
		can_seek &= is_wvc.seekable;
	}

	if (!can_seek) {
		open_flags |= OPEN_STREAMING;
	}

	wavpack_input_init(&isp, decoder, is);
	wpc = WavpackOpenFileInputEx(
		&mpd_is_reader, &isp,
		open_flags & OPEN_WVC ? &isp_wvc : NULL,
		error, open_flags, 23
	);

	if (wpc == NULL) {
		g_warning("failed to open WavPack stream: %s\n", error);
		return;
	}

	wavpack_decode(decoder, wpc, can_seek, NULL);

	WavpackCloseFile(wpc);
	if (open_flags & OPEN_WVC) {
		input_stream_close(&is_wvc);
	}
}

/*
 * Decodes a file.
 */
static void
wavpack_filedecode(struct decoder *decoder, const char *fname)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	struct replay_gain_info *replay_gain_info;

	wpc = WavpackOpenFileInput(
		fname, error,
		OPEN_TAGS | OPEN_WVC | OPEN_NORMALIZE, 23
	);
	if (wpc == NULL) {
		g_warning(
			"failed to open WavPack file \"%s\": %s\n",
			fname, error
		);
		return;
	}

	replay_gain_info = wavpack_replaygain(wpc);

	wavpack_decode(decoder, wpc, true, replay_gain_info);

	if (replay_gain_info) {
		replay_gain_info_free(replay_gain_info);
	}

	WavpackCloseFile(wpc);
}

static char const *const wavpack_suffixes[] = {
	"wv",
	NULL
};

static char const *const wavpack_mime_types[] = {
	"audio/x-wavpack",
	NULL
};

const struct decoder_plugin wavpack_decoder_plugin = {
	.name = "wavpack",
	.stream_decode = wavpack_streamdecode,
	.file_decode = wavpack_filedecode,
	.tag_dup = wavpack_tagdup,
	.suffixes = wavpack_suffixes,
	.mime_types = wavpack_mime_types
};
