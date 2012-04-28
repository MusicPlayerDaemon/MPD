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
 * This plugin decodes DSDIFF data (SACD) embedded in DFF and DSF files.
 * The DFF code was modeled after the specification found here:
 * http://www.sonicstudio.com/pdf/dsd/DSDIFF_1.5_Spec.pdf
 *
 * The DSF code was created using the specification found here:
 * http://dsd-guide.com/sonys-dsf-file-format-spec
 */

#include "config.h"
#include "dsdiff_decoder_plugin.h"
#include "decoder_api.h"
#include "audio_check.h"
#include "util/bit_reverse.h"

#include <unistd.h>
#include <stdio.h> /* for SEEK_SET, SEEK_CUR */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "dsdiff"

struct dsdiff_id {
	char value[4];
};

struct dsdiff_header {
	struct dsdiff_id id;
	uint32_t size_high, size_low;
	struct dsdiff_id format;
};

struct dsdiff_chunk_header {
	struct dsdiff_id id;
	uint32_t size_high, size_low;
};

struct dsdiff_metadata {
	unsigned sample_rate, channels;
	bool fileisdff;
	bool bitreverse;
	uint64_t chunk_size;
};

static bool lsbitfirst;


struct dsf_header {
	/** DSF header id: "DSD " */
	struct dsdiff_id id;
	/** DSD chunk size, including id = 28 */
	uint32_t size_low, size_high;
	/** Total file size */
	uint32_t fsize_low, fsize_high;
	/** Pointer to id3v2 metadata, should be at the end of the file */
	uint32_t pmeta_low, pmeta_high;
};
/** DSF file fmt chunk */
struct dsf_fmt_chunk {

	/** id: "fmt " */
	struct dsdiff_id id;
	/** fmt chunk size, including id, normally 52 */
	uint32_t size_low, size_high;
	/** Version of this format = 1 */
	uint32_t version;
	/** 0: DSD raw */
	uint32_t formatid;
	/** Channel Type, 1 = mono, 2 = stereo, 3 = 3 channels, etc */
	uint32_t channeltype;
	/** Channel number, 1 = mono, 2 = stereo, ... 6 = 6 channels */
	uint32_t channelnum;
	/** Sample frequency: 2822400, 5644800 */
	uint32_t sample_freq;
	/** Bits per sample 1 or 8 */
	uint32_t bitssample;
	/** Sample count per channel in bytes */
	uint32_t scnt_low, scnt_high;
	/** Block size per channel = 4096 */
	uint32_t block_size;
	/** Reserved, should be all zero */
	uint32_t reserved;
};

struct dsf_data_chunk {
	struct dsdiff_id id;
	/** "data" chunk size, includes header (id+size) */
	uint32_t size_low, size_high;
};

static bool
dsdiff_init(const struct config_param *param)
{
	lsbitfirst = config_get_block_bool(param, "lsbitfirst", false);
	return true;
}

static bool
dsdiff_id_equals(const struct dsdiff_id *id, const char *s)
{
	assert(id != NULL);
	assert(s != NULL);
	assert(strlen(s) == sizeof(id->value));

	return memcmp(id->value, s, sizeof(id->value)) == 0;
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
dsdiff_read(struct decoder *decoder, struct input_stream *is,
	    void *data, size_t length)
{
	size_t nbytes = decoder_read(decoder, is, data, length);
	return nbytes == length;
}

static bool
dsdiff_read_id(struct decoder *decoder, struct input_stream *is,
	       struct dsdiff_id *id)
{
	return dsdiff_read(decoder, is, id, sizeof(*id));
}

static bool
dsdiff_read_chunk_header(struct decoder *decoder, struct input_stream *is,
			 struct dsdiff_chunk_header *header)
{
	return dsdiff_read(decoder, is, header, sizeof(*header));
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
 * Skip the #input_stream to the specified offset.
 */
static bool
dsdiff_skip_to(struct decoder *decoder, struct input_stream *is,
	       goffset offset)
{
	if (is->seekable)
		return input_stream_seek(is, offset, SEEK_SET, NULL);

	if (is->offset > offset)
		return false;

	char buffer[8192];
	while (is->offset < offset) {
		size_t length = sizeof(buffer);
		if (offset - is->offset < (goffset)length)
			length = offset - is->offset;

		size_t nbytes = decoder_read(decoder, is, buffer, length);
		if (nbytes == 0)
			return false;
	}

	assert(is->offset == offset);
	return true;
}

/**
 * Skip some bytes from the #input_stream.
 */
static bool
dsdiff_skip(struct decoder *decoder, struct input_stream *is,
	    goffset delta)
{
	assert(delta >= 0);

	if (delta == 0)
		return true;

	if (is->seekable)
		return input_stream_seek(is, delta, SEEK_CUR, NULL);

	char buffer[8192];
	while (delta > 0) {
		size_t length = sizeof(buffer);
		if ((goffset)length > delta)
			length = delta;

		size_t nbytes = decoder_read(decoder, is, buffer, length);
		if (nbytes == 0)
			return false;

		delta -= nbytes;
	}

	return true;
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

		if (dsdiff_id_equals(&header.id, "FS  ")) {
			uint32_t sample_rate;
			if (!dsdiff_read_payload(decoder, is, &header,
						 &sample_rate,
						 sizeof(sample_rate)))
				return false;

			metadata->sample_rate = GUINT32_FROM_BE(sample_rate);
		} else if (dsdiff_id_equals(&header.id, "CHNL")) {
			uint16_t channels;
			if (dsdiff_chunk_size(&header) < sizeof(channels) ||
			    !dsdiff_read(decoder, is,
					 &channels, sizeof(channels)) ||
			    !dsdiff_skip_to(decoder, is, chunk_end_offset))
				return false;

			metadata->channels = GUINT16_FROM_BE(channels);
		} else if (dsdiff_id_equals(&header.id, "CMPR")) {
			struct dsdiff_id type;
			if (dsdiff_chunk_size(&header) < sizeof(type) ||
			    !dsdiff_read(decoder, is,
					 &type, sizeof(type)) ||
			    !dsdiff_skip_to(decoder, is, chunk_end_offset))
				return false;

			if (!dsdiff_id_equals(&type, "DSD "))
				/* only uncompressed DSD audio data
				   is implemented */
				return false;
		} else {
			/* ignore unknown chunk */

			if (!dsdiff_skip_to(decoder, is, chunk_end_offset))
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

	struct dsdiff_id prop_id;
	if (prop_size < sizeof(prop_id) ||
	    !dsdiff_read_id(decoder, is, &prop_id))
		return false;

	if (dsdiff_id_equals(&prop_id, "SND "))
		return dsdiff_read_prop_snd(decoder, is, metadata, end_offset);
	else
		/* ignore unknown PROP chunk */
		return dsdiff_skip_to(decoder, is, end_offset);
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
	if (!dsdiff_read(decoder, is, &header, sizeof(header)) ||
	    !dsdiff_id_equals(&header.id, "FRM8") ||
	    !dsdiff_id_equals(&header.format, "DSD "))
		return false;

	while (true) {
		if (!dsdiff_read_chunk_header(decoder, is,
					      chunk_header))
			return false;

		if (dsdiff_id_equals(&chunk_header->id, "PROP")) {
			if (!dsdiff_read_prop(decoder, is, metadata,
					      chunk_header))
					return false;
		} else if (dsdiff_id_equals(&chunk_header->id, "DSD ")) {
			/* done with metadata, mark as DFF */
			metadata->fileisdff = true;
			return true;
		} else {
			/* ignore unknown chunk */
			uint64_t chunk_size;
			chunk_size = dsdiff_chunk_size(chunk_header);
			goffset chunk_end_offset = is->offset + chunk_size;

			if (!dsdiff_skip_to(decoder, is,
					    chunk_end_offset))
				return false;
		}
	}
}

/**
 * Read and parse all needed metadata chunks for DSF files.
 */
static bool
dsf_read_metadata(struct decoder *decoder, struct input_stream *is,
		  struct dsdiff_metadata *metadata)
{
	/* Reset to beginning of the stream */
	if (!dsdiff_skip_to(decoder, is, 0))
		return false;

	uint64_t chunk_size;
	struct dsf_header dsf_header;
	if (!dsdiff_read(decoder, is, &dsf_header, sizeof(dsf_header)) ||
	    !dsdiff_id_equals(&dsf_header.id, "DSD "))
		return false;

	chunk_size = (((uint64_t)GUINT32_FROM_LE(dsf_header.size_high)) << 32) |
		((uint64_t)GUINT32_FROM_LE(dsf_header.size_low));

	if (sizeof(dsf_header) != chunk_size)
		return false;

	/* Read the 'fmt ' chunk of the DSF file */
	struct dsf_fmt_chunk dsf_fmt_chunk;
	if (!dsdiff_read(decoder, is, &dsf_fmt_chunk, sizeof(dsf_fmt_chunk)) ||
	    !dsdiff_id_equals(&dsf_fmt_chunk.id, "fmt "))
		return false;

	uint64_t fmt_chunk_size;
	fmt_chunk_size = (((uint64_t)GUINT32_FROM_LE(dsf_fmt_chunk.size_high)) << 32) |
		((uint64_t)GUINT32_FROM_LE(dsf_fmt_chunk.size_low));

	if (fmt_chunk_size != sizeof(dsf_fmt_chunk))
		return false;

	uint32_t samplefreq = (uint32_t)GUINT32_FROM_LE(dsf_fmt_chunk.sample_freq);

	/* For now, only support version 1 of the standard, DSD raw stereo
	   files with a sample freq of 2822400 Hz */

	if (dsf_fmt_chunk.version != 1 || dsf_fmt_chunk.formatid != 0
	    || dsf_fmt_chunk.channeltype != 2
	    || dsf_fmt_chunk.channelnum != 2
	    || samplefreq != 2822400)
		return false;

	uint32_t chblksize = (uint32_t)GUINT32_FROM_LE(dsf_fmt_chunk.block_size);
	/* According to the spec block size should always be 4096 */
	if (chblksize != 4096)
		return false;

	/* Read the 'data' chunk of the DSF file */
	struct dsf_data_chunk data_chunk;
	if (!dsdiff_read(decoder, is, &data_chunk, sizeof(data_chunk)) ||
	    !dsdiff_id_equals(&data_chunk.id, "data"))
		return false;

	/* Data size of DSF files are padded to multiple of 4096,
	   we use the actual data size as chunk size */

	uint64_t data_size;
	data_size = (((uint64_t)GUINT32_FROM_LE(data_chunk.size_high)) << 32) |
		((uint64_t)GUINT32_FROM_LE(data_chunk.size_low));
	data_size -= sizeof(data_chunk);

	metadata->chunk_size = data_size;
	metadata->channels = (unsigned) dsf_fmt_chunk.channelnum;
	metadata->sample_rate = samplefreq;

	/* Check bits per sample format, determine if bitreverse is needed */
	metadata->bitreverse = dsf_fmt_chunk.bitssample == 1 ?  true : false;
	metadata->fileisdff = false;
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
 * Decode one "DSD" chunk.
 */
static bool
dsdiff_decode_chunk(struct decoder *decoder, struct input_stream *is,
		    unsigned channels,
		    uint64_t chunk_size,
		    bool fileisdff,
		    bool bitreverse)
{
	uint8_t buffer[8192];

	/* Scratch buffer for DSF samples to convert to the needed
	   normal Left/Right regime of samples */
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

		if (lsbitfirst || bitreverse)
			bit_reverse_buffer(buffer, buffer + nbytes);

		if (!fileisdff)
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

	return dsdiff_skip(decoder, is, chunk_size);
}

static void
dsdiff_stream_decode(struct decoder *decoder, struct input_stream *is)
{
	struct dsdiff_metadata metadata = {
		.sample_rate = 0,
		.channels = 0,
	};

	struct dsdiff_chunk_header chunk_header;
	/* First see if it is is a DFF file */
	if (!dsdiff_read_metadata(decoder, is, &metadata, &chunk_header))
	{
		/* It was not a DFF file, now check if it is a DSF file */
		if (!dsf_read_metadata(decoder, is, &metadata))
			return;
	}

	GError *error = NULL;
	struct audio_format audio_format;
	if (!audio_format_init_checked(&audio_format, metadata.sample_rate / 8,
				       SAMPLE_FORMAT_DSD,
				       metadata.channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	/* success: file was recognized */
	decoder_initialized(decoder, &audio_format, false, -1);

	if (!metadata.fileisdff) {
		uint64_t chunk_size = metadata.chunk_size;
		if (!dsdiff_decode_chunk(decoder, is,
					 metadata.channels,
					 chunk_size,
					 metadata.fileisdff,
					 metadata.bitreverse))
			return;

	} else {

		/* every iteration of the following loop decodes one "DSD"
		   chunk from a DFF file */

		while (true) {

			uint64_t chunk_size = dsdiff_chunk_size(&chunk_header);

			if (dsdiff_id_equals(&chunk_header.id, "DSD ")) {
				if (!dsdiff_decode_chunk(decoder, is,
							 metadata.channels,
							 chunk_size,
							 metadata.fileisdff,
						/* Set bitreverse to
						   false for DFF files */
							false))
					break;
			} else {
				/* ignore other chunks */

				if (!dsdiff_skip(decoder, is, chunk_size))
					break;
			}

			/* read next chunk header; the first one was read by
			   dsdiff_read_metadata() */

			if (!dsdiff_read_chunk_header(decoder,
						      is, &chunk_header))
				break;
		}
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
	{
		/* It was not an DFF file, now check for DSF metadata */
		if (!dsf_read_metadata(NULL, is, &metadata))
			return false;
	}

	struct audio_format audio_format;
	if (!audio_format_init_checked(&audio_format, metadata.sample_rate / 8,
				       SAMPLE_FORMAT_DSD,
				       metadata.channels, NULL))
		/* refuse to parse files which we cannot play anyway */
		return false;

	/* no total time estimate, no tags implemented yet */
	return true;
}

static const char *const dsdiff_suffixes[] = {
	"dff",
	"dsf",
	NULL
};

static const char *const dsdiff_mime_types[] = {
	"application/x-dff",
	"application/x-dsf",
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
