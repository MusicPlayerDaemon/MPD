/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "tag_table.h"
#include "tag_handler.h"
#include "tag_ape.h"

#include <wavpack/wavpack.h>
#include <glib.h>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "wavpack"

#define ERRORLEN 80

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
		int8_t *dst = buffer;
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
	float *p = buffer;

	while (count--) {
		*p /= (1 << 23);
		++p;
	}
}

/**
 * Choose a MPD sample format from libwavpacks' number of bits.
 */
static enum sample_format
wavpack_bits_to_sample_format(bool is_float, int bytes_per_sample)
{
	if (is_float)
		return SAMPLE_FORMAT_FLOAT;

	switch (bytes_per_sample) {
	case 1:
		return SAMPLE_FORMAT_S8;

	case 2:
		return SAMPLE_FORMAT_S16;

	case 3:
		return SAMPLE_FORMAT_S24_P32;

	case 4:
		return SAMPLE_FORMAT_S32;

	default:
		return SAMPLE_FORMAT_UNDEFINED;
	}
}

/*
 * This does the main decoding thing.
 * Requires an already opened WavpackContext.
 */
static void
wavpack_decode(struct decoder *decoder, WavpackContext *wpc, bool can_seek)
{
	GError *error = NULL;
	bool is_float;
	enum sample_format sample_format;
	struct audio_format audio_format;
	format_samples_t format_samples;
	float total_time;
	int bytes_per_sample, output_sample_size;

	is_float = (WavpackGetMode(wpc) & MODE_FLOAT) != 0;
	sample_format =
		wavpack_bits_to_sample_format(is_float,
					      WavpackGetBytesPerSample(wpc));

	if (!audio_format_init_checked(&audio_format,
				       WavpackGetSampleRate(wpc),
				       sample_format,
				       WavpackGetNumChannels(wpc), &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	if (is_float) {
		format_samples = format_samples_float;
	} else {
		format_samples = format_samples_int;
	}

	total_time = WavpackGetNumSamples(wpc);
	total_time /= audio_format.sample_rate;
	bytes_per_sample = WavpackGetBytesPerSample(wpc);
	output_sample_size = audio_format_frame_size(&audio_format);

	/* wavpack gives us all kind of samples in a 32-bit space */
	int32_t chunk[1024];
	const uint32_t samples_requested = G_N_ELEMENTS(chunk) /
		audio_format.channels;

	decoder_initialized(decoder, &audio_format, can_seek, total_time);

	enum decoder_command cmd = decoder_get_command(decoder);
	while (cmd != DECODE_COMMAND_STOP) {
		if (cmd == DECODE_COMMAND_SEEK) {
			if (can_seek) {
				unsigned where = decoder_seek_where(decoder) *
					audio_format.sample_rate;

				if (WavpackSeekSample(wpc, where)) {
					decoder_command_finished(decoder);
				} else {
					decoder_seek_error(decoder);
				}
			} else {
				decoder_seek_error(decoder);
			}
		}

		uint32_t samples_got = WavpackUnpackSamples(wpc, chunk,
							    samples_requested);
		if (samples_got == 0)
			break;

		int bitrate = (int)(WavpackGetInstantBitrate(wpc) / 1000 +
				    0.5);
		format_samples(bytes_per_sample, chunk,
			       samples_got * audio_format.channels);

		cmd = decoder_data(decoder, NULL, chunk,
				   samples_got * output_sample_size,
				   bitrate);
	}
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

static bool
wavpack_replaygain(struct replay_gain_info *replay_gain_info,
		   WavpackContext *wpc)
{
	bool found = false;

	replay_gain_info_init(replay_gain_info);

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

	return found;
}

static void
wavpack_scan_tag_item(WavpackContext *wpc, const char *name,
		      enum tag_type type,
		      const struct tag_handler *handler, void *handler_ctx)
{
	char buffer[1024];
	int len = WavpackGetTagItem(wpc, name, buffer, sizeof(buffer));
	if (len <= 0 || (unsigned)len >= sizeof(buffer))
		return;

	tag_handler_invoke_tag(handler, handler_ctx, type, buffer);

}

static void
wavpack_scan_pair(WavpackContext *wpc, const char *name,
		  const struct tag_handler *handler, void *handler_ctx)
{
	char buffer[8192];
	int len = WavpackGetTagItem(wpc, name, buffer, sizeof(buffer));
	if (len <= 0 || (unsigned)len >= sizeof(buffer))
		return;

	tag_handler_invoke_pair(handler, handler_ctx, name, buffer);
}

/*
 * Reads metainfo from the specified file.
 */
static bool
wavpack_scan_file(const char *fname,
		  const struct tag_handler *handler, void *handler_ctx)
{
	WavpackContext *wpc;
	char error[ERRORLEN];

	wpc = WavpackOpenFileInput(fname, error, OPEN_TAGS, 0);
	if (wpc == NULL) {
		g_warning(
			"failed to open WavPack file \"%s\": %s\n",
			fname, error
		);
		return false;
	}

	tag_handler_invoke_duration(handler, handler_ctx,
				    WavpackGetNumSamples(wpc) /
				    WavpackGetSampleRate(wpc));

	/* the WavPack format implies APEv2 tags, which means we can
	   reuse the mapping from tag_ape.c */

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		const char *name = tag_item_names[i];
		if (name != NULL)
			wavpack_scan_tag_item(wpc, name, (enum tag_type)i,
					      handler, handler_ctx);
	}

	for (const struct tag_table *i = ape_tags; i->name != NULL; ++i)
		wavpack_scan_tag_item(wpc, i->name, i->type,
				      handler, handler_ctx);

	if (handler->pair != NULL) {
		char name[64];

		for (int i = 0, n = WavpackGetNumTagItems(wpc);
		     i < n; ++i) {
			int len = WavpackGetTagItemIndexed(wpc, i, name,
							   sizeof(name));
			if (len <= 0 || (unsigned)len >= sizeof(name))
				continue;

			wavpack_scan_pair(wpc, name, handler, handler_ctx);
		}
	}

	WavpackCloseFile(wpc);

	return true;
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
	return input_stream_lock_seek(wpin(id)->is, pos, SEEK_SET, NULL)
		? 0 : -1;
}

static int
wavpack_input_set_pos_rel(void *id, int32_t delta, int mode)
{
	return input_stream_lock_seek(wpin(id)->is, delta, mode, NULL)
		? 0 : -1;
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

static struct input_stream *
wavpack_open_wvc(struct decoder *decoder, const char *uri,
		 GMutex *mutex, GCond *cond,
		 struct wavpack_input *wpi)
{
	struct input_stream *is_wvc;
	char *wvc_url = NULL;
	char first_byte;
	size_t nbytes;

	/*
	 * As we use dc->utf8url, this function will be bad for
	 * single files. utf8url is not absolute file path :/
	 */
	if (uri == NULL)
		return false;

	wvc_url = g_strconcat(uri, "c", NULL);
	is_wvc = input_stream_open(wvc_url, mutex, cond, NULL);
	g_free(wvc_url);

	if (is_wvc == NULL)
		return NULL;

	/*
	 * And we try to buffer in order to get know
	 * about a possible 404 error.
	 */
	nbytes = decoder_read(
		decoder, is_wvc, &first_byte, sizeof(first_byte)
	);
	if (nbytes == 0) {
		input_stream_close(is_wvc);
		return NULL;
	}

	/* push it back */
	wavpack_input_init(wpi, decoder, is_wvc);
	wpi->last_byte = first_byte;
	return is_wvc;
}

/*
 * Decodes a stream.
 */
static void
wavpack_streamdecode(struct decoder * decoder, struct input_stream *is)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	struct input_stream *is_wvc;
	int open_flags = OPEN_NORMALIZE;
	struct wavpack_input isp, isp_wvc;
	bool can_seek = is->seekable;

	is_wvc = wavpack_open_wvc(decoder, is->uri, is->mutex, is->cond,
				  &isp_wvc);
	if (is_wvc != NULL) {
		open_flags |= OPEN_WVC;
		can_seek &= is_wvc->seekable;
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

	wavpack_decode(decoder, wpc, can_seek);

	WavpackCloseFile(wpc);
	if (open_flags & OPEN_WVC) {
		input_stream_close(is_wvc);
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

	struct replay_gain_info replay_gain_info;
	if (wavpack_replaygain(&replay_gain_info, wpc))
		decoder_replay_gain(decoder, &replay_gain_info);

	wavpack_decode(decoder, wpc, true);

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
	.scan_file = wavpack_scan_file,
	.suffixes = wavpack_suffixes,
	.mime_types = wavpack_mime_types
};
