/*
 * Copyright (C) 2012 The Music Player Daemon Project
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
 * This plugin decodes DSDIFF data (SACD) embedded in DSF files.
 *
 * The DSF code was created using the specification found here:
 * http://dsd-guide.com/sonys-dsf-file-format-spec
 *
 * All functions common to both DSD decoders have been moved to dsdlib
 */

#include "config.h"
#include "dsf_decoder_plugin.h"
#include "decoder_api.h"
#include "audio_check.h"
#include "util/bit_reverse.h"
#include "dsdlib.h"
#include "tag_handler.h"

#include <unistd.h>
#include <stdio.h> /* for SEEK_SET, SEEK_CUR */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "dsf"

struct dsf_metadata {
	unsigned sample_rate, channels;
	bool bitreverse;
	uint64_t chunk_size;
};

struct dsf_header {
	/** DSF header id: "DSD " */
	struct dsdlib_id id;
	/** DSD chunk size, including id = 28 */
	uint32_t size_low, size_high;
	/** total file size */
	uint32_t fsize_low, fsize_high;
	/** pointer to id3v2 metadata, should be at the end of the file */
	uint32_t pmeta_low, pmeta_high;
};
/** DSF file fmt chunk */
struct dsf_fmt_chunk {

	/** id: "fmt " */
	struct dsdlib_id id;
	/** fmt chunk size, including id, normally 52 */
	uint32_t size_low, size_high;
	/** version of this format = 1 */
	uint32_t version;
	/** 0: DSD raw */
	uint32_t formatid;
	/** channel type, 1 = mono, 2 = stereo, 3 = 3 channels, etc */
	uint32_t channeltype;
	/** Channel number, 1 = mono, 2 = stereo, ... 6 = 6 channels */
	uint32_t channelnum;
	/** sample frequency: 2822400, 5644800 */
	uint32_t sample_freq;
	/** bits per sample 1 or 8 */
	uint32_t bitssample;
	/** Sample count per channel in bytes */
	uint32_t scnt_low, scnt_high;
	/** block size per channel = 4096 */
	uint32_t block_size;
	/** reserved, should be all zero */
	uint32_t reserved;
};

struct dsf_data_chunk {
	struct dsdlib_id id;
	/** "data" chunk size, includes header (id+size) */
	uint32_t size_low, size_high;
};

/**
 * Read and parse all needed metadata chunks for DSF files.
 */
static bool
dsf_read_metadata(struct decoder *decoder, struct input_stream *is,
		  struct dsf_metadata *metadata)
{
	uint64_t chunk_size;
	struct dsf_header dsf_header;
	if (!dsdlib_read(decoder, is, &dsf_header, sizeof(dsf_header)) ||
	    !dsdlib_id_equals(&dsf_header.id, "DSD "))
		return false;

	chunk_size = (((uint64_t)GUINT32_FROM_LE(dsf_header.size_high)) << 32) |
		((uint64_t)GUINT32_FROM_LE(dsf_header.size_low));

	if (sizeof(dsf_header) != chunk_size)
		return false;

	/* read the 'fmt ' chunk of the DSF file */
	struct dsf_fmt_chunk dsf_fmt_chunk;
	if (!dsdlib_read(decoder, is, &dsf_fmt_chunk, sizeof(dsf_fmt_chunk)) ||
	    !dsdlib_id_equals(&dsf_fmt_chunk.id, "fmt "))
		return false;

	uint64_t fmt_chunk_size;
	fmt_chunk_size = (((uint64_t)GUINT32_FROM_LE(dsf_fmt_chunk.size_high)) << 32) |
			  ((uint64_t)GUINT32_FROM_LE(dsf_fmt_chunk.size_low));

	if (fmt_chunk_size != sizeof(dsf_fmt_chunk))
		return false;

	uint32_t samplefreq = (uint32_t)GUINT32_FROM_LE(dsf_fmt_chunk.sample_freq);

	/* for now, only support version 1 of the standard, DSD raw stereo
	   files with a sample freq of 2822400 Hz */

	if (dsf_fmt_chunk.version != 1 || dsf_fmt_chunk.formatid != 0
	    || dsf_fmt_chunk.channeltype != 2
	    || dsf_fmt_chunk.channelnum != 2
	    || samplefreq != 2822400)
		return false;

	uint32_t chblksize = (uint32_t)GUINT32_FROM_LE(dsf_fmt_chunk.block_size);
	/* according to the spec block size should always be 4096 */
	if (chblksize != 4096)
		return false;

	/* read the 'data' chunk of the DSF file */
	struct dsf_data_chunk data_chunk;
	if (!dsdlib_read(decoder, is, &data_chunk, sizeof(data_chunk)) ||
	    !dsdlib_id_equals(&data_chunk.id, "data"))
		return false;

	/* data size of DSF files are padded to multiple of 4096,
	   we use the actual data size as chunk size */

	uint64_t data_size;
	data_size = (((uint64_t)GUINT32_FROM_LE(data_chunk.size_high)) << 32) |
		((uint64_t)GUINT32_FROM_LE(data_chunk.size_low));
	data_size -= sizeof(data_chunk);

	metadata->chunk_size = data_size;
	metadata->channels = (unsigned) dsf_fmt_chunk.channelnum;
	metadata->sample_rate = samplefreq;

	/* check bits per sample format, determine if bitreverse is needed */
	metadata->bitreverse = dsf_fmt_chunk.bitssample == 1;
	return true;
}

static void
bit_reverse_buffer(uint8_t *p, uint8_t *end)
{
	for (; p < end; ++p)
		*p = bit_reverse(*p);
}

/**
 * DSF data is build up of alternating 4096 blocks of DSD samples for left and
 * right. Convert the buffer holding 1 block of 4096 DSD left samples and 1
 * block of 4096 DSD right samples to 8k of samples in normal PCM left/right
 * order.
 */
static void
dsf_to_pcm_order(uint8_t *dest, uint8_t *scratch, size_t nrbytes)
{
	for (unsigned i = 0, j = 0; i < (unsigned)nrbytes; i += 2) {
		scratch[i] = *(dest+j);
		j++;
	}

	for (unsigned i = 1, j = 0; i < (unsigned) nrbytes; i += 2) {
		scratch[i] = *(dest+4096+j);
		j++;
	}

	for (unsigned i = 0; i < (unsigned)nrbytes; i++) {
		*dest = scratch[i];
		dest++;
	}
}

/**
 * Decode one complete DSF 'data' chunk i.e. a complete song
 */
static bool
dsf_decode_chunk(struct decoder *decoder, struct input_stream *is,
		    unsigned channels,
		    uint64_t chunk_size,
		    bool bitreverse)
{
	uint8_t buffer[8192];

	/* scratch buffer for DSF samples to convert to the needed
	   normal left/right regime of samples */
	uint8_t dsf_scratch_buffer[8192];

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

		if (bitreverse)
			bit_reverse_buffer(buffer, buffer + nbytes);

		dsf_to_pcm_order(buffer, dsf_scratch_buffer, nbytes);

		enum decoder_command cmd =
			decoder_data(decoder, is, buffer, nbytes, 0);
		switch (cmd) {
		case DECODE_COMMAND_NONE:
			break;

		case DECODE_COMMAND_START:
		case DECODE_COMMAND_STOP:
			return false;

		case DECODE_COMMAND_SEEK:

			/* not implemented yet */
			decoder_seek_error(decoder);
			break;
			}
	}
	return dsdlib_skip(decoder, is, chunk_size);
}

static void
dsf_stream_decode(struct decoder *decoder, struct input_stream *is)
{
	struct dsf_metadata metadata = {
		.sample_rate = 0,
		.channels = 0,
	};

	/* check if it is a proper DSF file */
	if (!dsf_read_metadata(decoder, is, &metadata))
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
	/* Calculate song time from DSD chunk size and sample frequency */
	uint64_t chunk_size = metadata.chunk_size;
	float songtime = ((chunk_size / metadata.channels) * 8) /
			 (float) metadata.sample_rate;

	/* success: file was recognized */
	decoder_initialized(decoder, &audio_format, false, songtime);

	if (!dsf_decode_chunk(decoder, is, metadata.channels,
			      metadata.chunk_size,
			      metadata.bitreverse))
		return;
}

static bool
dsf_scan_stream(struct input_stream *is,
		   G_GNUC_UNUSED const struct tag_handler *handler,
		   G_GNUC_UNUSED void *handler_ctx)
{
	struct dsf_metadata metadata = {
		.sample_rate = 0,
		.channels = 0,
	};

	/* check DSF metadata */
	if (!dsf_read_metadata(NULL, is, &metadata))
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

static const char *const dsf_suffixes[] = {
	"dsf",
	NULL
};

static const char *const dsf_mime_types[] = {
	"application/x-dsf",
	NULL
};

const struct decoder_plugin dsf_decoder_plugin = {
	.name = "dsf",
	.stream_decode = dsf_stream_decode,
	.scan_stream = dsf_scan_stream,
	.suffixes = dsf_suffixes,
	.mime_types = dsf_mime_types,
};
