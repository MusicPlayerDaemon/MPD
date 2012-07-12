/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

/* \file
 *
 * This plugin decodes DSDIFF data (SACD) embedded in DFF files.
 * The DFF code was modeled after the specification found here:
 * http://www.sonicstudio.com/pdf/dsd/DSDIFF_1.5_Spec.pdf
 *
 * All functions common to both DSD decoders have been moved to dsdlib
 */

#include "config.h"
#include "dsdiff_decoder_plugin.h"
#include "decoder_api.h"
#include "audio_check.h"
#include "util/bit_reverse.h"
#include "tag_handler.h"
#include "dsdlib.h"
#include "tag_handler.h"

#include <unistd.h>
#include <stdio.h> /* for SEEK_SET, SEEK_CUR */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "dsdiff"

struct dsdiff_header {
	struct dsdlib_id id;
	uint32_t size_high, size_low;
	struct dsdlib_id format;
};

struct dsdiff_chunk_header {
	struct dsdlib_id id;
	uint32_t size_high, size_low;
};

struct dsdiff_metadata {
	unsigned sample_rate, channels;
	bool bitreverse;
	uint64_t chunk_size;
};

static bool lsbitfirst;

static bool
dsdiff_init(const struct config_param *param)
{
	lsbitfirst = config_get_block_bool(param, "lsbitfirst", false);
	return true;
}

/**
 * Read the "size" attribute from the specified header, converting it
 * to the host byte order if needed.
 */
G_GNUC_CONST
static uint64_t
dsdiff_chunk_size(const struct dsdiff_chunk_header *header)
{
	return (((uint64_t)GUINT32_FROM_BE(header->size_high)) << 32) |
		((uint64_t)GUINT32_FROM_BE(header->size_low));
}

static bool
dsdiff_read_id(struct decoder *decoder, struct input_stream *is,
	       struct dsdlib_id *id)
{
	return dsdlib_read(decoder, is, id, sizeof(*id));
}

static bool
dsdiff_read_chunk_header(struct decoder *decoder, struct input_stream *is,
			 struct dsdiff_chunk_header *header)
{
	return dsdlib_read(decoder, is, header, sizeof(*header));
}

static bool
dsdiff_read_payload(struct decoder *decoder, struct input_stream *is,
		    const struct dsdiff_chunk_header *header,
		    void *data, size_t length)
{
	uint64_t size = dsdiff_chunk_size(header);
	if (size != (uint64_t)length)
		return false;

	size_t nbytes = decoder_read(decoder, is, data, length);
	return nbytes == length;
}

/**
 * Read and parse a "SND" chunk inside "PROP".
 */
static bool
dsdiff_read_prop_snd(struct decoder *decoder, struct input_stream *is,
		     struct dsdiff_metadata *metadata,
		     goffset end_offset)
{
	struct dsdiff_chunk_header header;
	while ((goffset)(is->offset + sizeof(header)) <= end_offset) {
		if (!dsdiff_read_chunk_header(decoder, is, &header))
			return false;

		goffset chunk_end_offset =
			is->offset + dsdiff_chunk_size(&header);
		if (chunk_end_offset > end_offset)
			return false;

		if (dsdlib_id_equals(&header.id, "FS  ")) {
			uint32_t sample_rate;
			if (!dsdiff_read_payload(decoder, is, &header,
						 &sample_rate,
						 sizeof(sample_rate)))
				return false;

			metadata->sample_rate = GUINT32_FROM_BE(sample_rate);
		} else if (dsdlib_id_equals(&header.id, "CHNL")) {
			uint16_t channels;
			if (dsdiff_chunk_size(&header) < sizeof(channels) ||
			    !dsdlib_read(decoder, is,
					 &channels, sizeof(channels)) ||
			    !dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;

			metadata->channels = GUINT16_FROM_BE(channels);
		} else if (dsdlib_id_equals(&header.id, "CMPR")) {
			struct dsdlib_id type;
			if (dsdiff_chunk_size(&header) < sizeof(type) ||
			    !dsdlib_read(decoder, is,
					 &type, sizeof(type)) ||
			    !dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;

			if (!dsdlib_id_equals(&type, "DSD "))
				/* only uncompressed DSD audio data
				   is implemented */
				return false;
		} else {
			/* ignore unknown chunk */

			if (!dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;
		}
	}

	return is->offset == end_offset;
}

/**
 * Read and parse a "PROP" chunk.
 */
static bool
dsdiff_read_prop(struct decoder *decoder, struct input_stream *is,
		 struct dsdiff_metadata *metadata,
		 const struct dsdiff_chunk_header *prop_header)
{
	uint64_t prop_size = dsdiff_chunk_size(prop_header);
	goffset end_offset = is->offset + prop_size;

	struct dsdlib_id prop_id;
	if (prop_size < sizeof(prop_id) ||
	    !dsdiff_read_id(decoder, is, &prop_id))
		return false;

	if (dsdlib_id_equals(&prop_id, "SND "))
		return dsdiff_read_prop_snd(decoder, is, metadata, end_offset);
	else
		/* ignore unknown PROP chunk */
		return dsdlib_skip_to(decoder, is, end_offset);
}

/**
 * Read and parse all metadata chunks at the beginning.  Stop when the
 * first "DSD" chunk is seen, and return its header in the
 * "chunk_header" parameter.
 */
static bool
dsdiff_read_metadata(struct decoder *decoder, struct input_stream *is,
		     struct dsdiff_metadata *metadata,
		     struct dsdiff_chunk_header *chunk_header)
{
	struct dsdiff_header header;
	if (!dsdlib_read(decoder, is, &header, sizeof(header)) ||
	    !dsdlib_id_equals(&header.id, "FRM8") ||
	    !dsdlib_id_equals(&header.format, "DSD "))
		return false;

	while (true) {
		if (!dsdiff_read_chunk_header(decoder, is,
					      chunk_header))
			return false;

		if (dsdlib_id_equals(&chunk_header->id, "PROP")) {
			if (!dsdiff_read_prop(decoder, is, metadata,
					      chunk_header))
					return false;
		} else if (dsdlib_id_equals(&chunk_header->id, "DSD ")) {
			uint64_t chunk_size;
			chunk_size = dsdiff_chunk_size(chunk_header);
			metadata->chunk_size = chunk_size;
			return true;
		} else {
			/* ignore unknown chunk */
			uint64_t chunk_size;
			chunk_size = dsdiff_chunk_size(chunk_header);
			goffset chunk_end_offset = is->offset + chunk_size;

			if (!dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;
		}
	}
}

static void
bit_reverse_buffer(uint8_t *p, uint8_t *end)
{
	for (; p < end; ++p)
		*p = bit_reverse(*p);
}

/**
 * Decode one "DSD" chunk.
 */
static bool
dsdiff_decode_chunk(struct decoder *decoder, struct input_stream *is,
		    unsigned channels,
		    uint64_t chunk_size)
{
	uint8_t buffer[8192];

	const size_t sample_size = sizeof(buffer[0]);
	const size_t frame_size = channels * sample_size;
	const unsigned buffer_frames = sizeof(buffer) / frame_size;
	const unsigned buffer_samples = buffer_frames * frame_size;
	const size_t buffer_size = buffer_samples * sample_size;

	while (chunk_size > 0) {
		/* see how much aligned data from the remaining chunk
		   fits into the local buffer */
		unsigned now_frames = buffer_frames;
		size_t now_size = buffer_size;
		if (chunk_size < (uint64_t)now_size) {
			now_frames = (unsigned)chunk_size / frame_size;
			now_size = now_frames * frame_size;
		}

		size_t nbytes = decoder_read(decoder, is, buffer, now_size);
		if (nbytes != now_size)
			return false;

		chunk_size -= nbytes;

		if (lsbitfirst)
			bit_reverse_buffer(buffer, buffer + nbytes);

		enum decoder_command cmd =
			decoder_data(decoder, is, buffer, nbytes, 0);
		switch (cmd) {
		case DECODE_COMMAND_NONE:
			break;

		case DECODE_COMMAND_START:
		case DECODE_COMMAND_STOP:
			return false;

		case DECODE_COMMAND_SEEK:

			/* Not implemented yet */
			decoder_seek_error(decoder);
			break;
		}
	}
	return dsdlib_skip(decoder, is, chunk_size);
}

static void
dsdiff_stream_decode(struct decoder *decoder, struct input_stream *is)
{
	struct dsdiff_metadata metadata = {
		.sample_rate = 0,
		.channels = 0,
	};

	struct dsdiff_chunk_header chunk_header;
	/* check if it is is a proper DFF file */
	if (!dsdiff_read_metadata(decoder, is, &metadata, &chunk_header))
		return;

	GError *error = NULL;
	struct audio_format audio_format;
	if (!audio_format_init_checked(&audio_format, metadata.sample_rate / 8,
				       SAMPLE_FORMAT_DSD,
				       metadata.channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	/* calculate song time from DSD chunk size and sample frequency */
	uint64_t chunk_size = metadata.chunk_size;
	float songtime = ((chunk_size / metadata.channels) * 8) /
			 (float) metadata.sample_rate;

	/* success: file was recognized */
	decoder_initialized(decoder, &audio_format, false, songtime);

	/* every iteration of the following loop decodes one "DSD"
	   chunk from a DFF file */

	while (true) {
		chunk_size = dsdiff_chunk_size(&chunk_header);

		if (dsdlib_id_equals(&chunk_header.id, "DSD ")) {
			if (!dsdiff_decode_chunk(decoder, is,
						 metadata.channels,
						 chunk_size))
					break;
		} else {
			/* ignore other chunks */
			if (!dsdlib_skip(decoder, is, chunk_size))
				break;
		}

		/* read next chunk header; the first one was read by
		   dsdiff_read_metadata() */
		if (!dsdiff_read_chunk_header(decoder,
					      is, &chunk_header))
			break;
	}
}

static bool
dsdiff_scan_stream(struct input_stream *is,
		   G_GNUC_UNUSED const struct tag_handler *handler,
		   G_GNUC_UNUSED void *handler_ctx)
{
	struct dsdiff_metadata metadata = {
		.sample_rate = 0,
		.channels = 0,
	};

	struct dsdiff_chunk_header chunk_header;
	/* First check for DFF metadata */
	if (!dsdiff_read_metadata(NULL, is, &metadata, &chunk_header))
		return false;

	struct audio_format audio_format;
	if (!audio_format_init_checked(&audio_format, metadata.sample_rate / 8,
				       SAMPLE_FORMAT_DSD,
				       metadata.channels, NULL))
		/* refuse to parse files which we cannot play anyway */
		return false;

	/* calculate song time and add as tag */
	unsigned songtime = ((metadata.chunk_size / metadata.channels) * 8) /
			    metadata.sample_rate;
	tag_handler_invoke_duration(handler, handler_ctx, songtime);

	return true;
}

static const char *const dsdiff_suffixes[] = {
	"dff",
	NULL
};

static const char *const dsdiff_mime_types[] = {
	"application/x-dff",
	NULL
};

const struct decoder_plugin dsdiff_decoder_plugin = {
	.name = "dsdiff",
	.init = dsdiff_init,
	.stream_decode = dsdiff_stream_decode,
	.scan_stream = dsdiff_scan_stream,
	.suffixes = dsdiff_suffixes,
	.mime_types = dsdiff_mime_types,
};
