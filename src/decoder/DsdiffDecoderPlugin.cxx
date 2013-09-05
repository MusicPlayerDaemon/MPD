/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DsdiffDecoderPlugin.hxx"
#include "DecoderAPI.hxx"
#include "InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "util/bit_reverse.h"
#include "util/Error.hxx"
#include "tag/TagHandler.hxx"
#include "DsdLib.hxx"

#include <unistd.h>
#include <stdio.h> /* for SEEK_SET, SEEK_CUR */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "dsdiff"

struct DsdiffHeader {
	struct dsdlib_id id;
	uint32_t size_high, size_low;
	struct dsdlib_id format;
};

struct DsdiffChunkHeader {
	struct dsdlib_id id;
	uint32_t size_high, size_low;

	/**
	 * Read the "size" attribute from the specified header, converting it
	 * to the host byte order if needed.
	 */
	gcc_const
	uint64_t GetSize() const {
		return (((uint64_t)GUINT32_FROM_BE(size_high)) << 32) |
			((uint64_t)GUINT32_FROM_BE(size_low));
	}
};

/** struct for DSDIFF native Artist and Title tags */
struct dsdiff_native_tag {
	uint32_t size;
};

struct DsdiffMetaData {
	unsigned sample_rate, channels;
	bool bitreverse;
	uint64_t chunk_size;
#ifdef HAVE_ID3TAG
	goffset id3_offset;
	uint64_t id3_size;
#endif
	/** offset for artist tag */
	goffset diar_offset;
	/** offset for title tag */
	goffset diti_offset;
};

static bool lsbitfirst;

static bool
dsdiff_init(const config_param &param)
{
	lsbitfirst = param.GetBlockValue("lsbitfirst", false);
	return true;
}

static bool
dsdiff_read_id(struct decoder *decoder, struct input_stream *is,
	       struct dsdlib_id *id)
{
	return dsdlib_read(decoder, is, id, sizeof(*id));
}

static bool
dsdiff_read_chunk_header(struct decoder *decoder, struct input_stream *is,
			 DsdiffChunkHeader *header)
{
	return dsdlib_read(decoder, is, header, sizeof(*header));
}

static bool
dsdiff_read_payload(struct decoder *decoder, struct input_stream *is,
		    const DsdiffChunkHeader *header,
		    void *data, size_t length)
{
	uint64_t size = header->GetSize();
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
		     DsdiffMetaData *metadata,
		     goffset end_offset)
{
	DsdiffChunkHeader header;
	while ((goffset)(is->GetOffset() + sizeof(header)) <= end_offset) {
		if (!dsdiff_read_chunk_header(decoder, is, &header))
			return false;

		goffset chunk_end_offset = is->GetOffset()
			+ header.GetSize();
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
			if (header.GetSize() < sizeof(channels) ||
			    !dsdlib_read(decoder, is,
					 &channels, sizeof(channels)) ||
			    !dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;

			metadata->channels = GUINT16_FROM_BE(channels);
		} else if (dsdlib_id_equals(&header.id, "CMPR")) {
			struct dsdlib_id type;
			if (header.GetSize() < sizeof(type) ||
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

	return is->GetOffset() == end_offset;
}

/**
 * Read and parse a "PROP" chunk.
 */
static bool
dsdiff_read_prop(struct decoder *decoder, struct input_stream *is,
		 DsdiffMetaData *metadata,
		 const DsdiffChunkHeader *prop_header)
{
	uint64_t prop_size = prop_header->GetSize();
	goffset end_offset = is->GetOffset() + prop_size;

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

static void
dsdiff_handle_native_tag(struct input_stream *is,
			 const struct tag_handler *handler,
			 void *handler_ctx, goffset tagoffset,
			 enum tag_type type)
{
	if (!dsdlib_skip_to(nullptr, is, tagoffset))
		return;

	struct dsdiff_native_tag metatag;

	if (!dsdlib_read(nullptr, is, &metatag, sizeof(metatag)))
		return;

	uint32_t length = GUINT32_FROM_BE(metatag.size);

	/* Check and limit size of the tag to prevent a stack overflow */
	if (length == 0 || length > 60)
		return;

	char string[length];
	char *label;
	label = string;

	if (!dsdlib_read(nullptr, is, label, (size_t)length))
		return;

	string[length] = '\0';
	tag_handler_invoke_tag(handler, handler_ctx, type, label);
	return;
}

/**
 * Read and parse additional metadata chunks for tagging purposes. By default
 * dsdiff files only support equivalents for artist and title but some of the
 * extract tools add an id3 tag to provide more tags. If such id3 is found
 * this will be used for tagging otherwise the native tags (if any) will be
 * used
 */

static bool
dsdiff_read_metadata_extra(struct decoder *decoder, struct input_stream *is,
			   DsdiffMetaData *metadata,
			   DsdiffChunkHeader *chunk_header,
			   const struct tag_handler *handler,
			   void *handler_ctx)
{

	/* skip from DSD data to next chunk header */
	if (!dsdlib_skip(decoder, is, metadata->chunk_size))
		return false;
	if (!dsdiff_read_chunk_header(decoder, is, chunk_header))
		return false;

#ifdef HAVE_ID3TAG
	metadata->id3_size = 0;
#endif

	/* Now process all the remaining chunk headers in the stream
	   and record their position and size */

	const goffset size = is->GetSize();
	while (is->GetOffset() < size) {
		uint64_t chunk_size = chunk_header->GetSize();

		/* DIIN chunk, is directly followed by other chunks  */
		if (dsdlib_id_equals(&chunk_header->id, "DIIN"))
			chunk_size = 0;

		/* DIAR chunk - DSDIFF native tag for Artist */
		if (dsdlib_id_equals(&chunk_header->id, "DIAR")) {
			chunk_size = chunk_header->GetSize();
			metadata->diar_offset = is->GetOffset();
		}

		/* DITI chunk - DSDIFF native tag for Title */
		if (dsdlib_id_equals(&chunk_header->id, "DITI")) {
			chunk_size = chunk_header->GetSize();
			metadata->diti_offset = is->GetOffset();
		}
#ifdef HAVE_ID3TAG
		/* 'ID3 ' chunk, offspec. Used by sacdextract */
		if (dsdlib_id_equals(&chunk_header->id, "ID3 ")) {
			chunk_size = chunk_header->GetSize();
			metadata->id3_offset = is->GetOffset();
			metadata->id3_size = chunk_size;
		}
#endif
		if (chunk_size != 0) {
			if (!dsdlib_skip(decoder, is, chunk_size))
				break;
		}

		if (is->GetOffset() < size) {
			if (!dsdiff_read_chunk_header(decoder, is, chunk_header))
				return false;
		}
		chunk_size = 0;
	}
	/* done processing chunk headers, process tags if any */

#ifdef HAVE_ID3TAG
	if (metadata->id3_offset != 0)
	{
		/* a ID3 tag has preference over the other tags, do not process
		   other tags if we have one */
		dsdlib_tag_id3(is, handler, handler_ctx, metadata->id3_offset);
		return true;
	}
#endif

	if (metadata->diar_offset != 0)
		dsdiff_handle_native_tag(is, handler, handler_ctx,
					 metadata->diar_offset, TAG_ARTIST);

	if (metadata->diti_offset != 0)
		dsdiff_handle_native_tag(is, handler, handler_ctx,
					 metadata->diti_offset, TAG_TITLE);
	return true;
}

/**
 * Read and parse all metadata chunks at the beginning.  Stop when the
 * first "DSD" chunk is seen, and return its header in the
 * "chunk_header" parameter.
 */
static bool
dsdiff_read_metadata(struct decoder *decoder, struct input_stream *is,
		     DsdiffMetaData *metadata,
		     DsdiffChunkHeader *chunk_header)
{
	DsdiffHeader header;
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
			const uint64_t chunk_size = chunk_header->GetSize();
			metadata->chunk_size = chunk_size;
			return true;
		} else {
			/* ignore unknown chunk */
			const uint64_t chunk_size = chunk_header->GetSize();
			goffset chunk_end_offset = is->GetOffset()
				+ chunk_size;

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
	DsdiffMetaData metadata;

	DsdiffChunkHeader chunk_header;
	/* check if it is is a proper DFF file */
	if (!dsdiff_read_metadata(decoder, is, &metadata, &chunk_header))
		return;

	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, metadata.sample_rate / 8,
				       SampleFormat::DSD,
				       metadata.channels, error)) {
		g_warning("%s", error.GetMessage());
		return;
	}

	/* calculate song time from DSD chunk size and sample frequency */
	uint64_t chunk_size = metadata.chunk_size;
	float songtime = ((chunk_size / metadata.channels) * 8) /
			 (float) metadata.sample_rate;

	/* success: file was recognized */
	decoder_initialized(decoder, audio_format, false, songtime);

	/* every iteration of the following loop decodes one "DSD"
	   chunk from a DFF file */

	while (true) {
		chunk_size = chunk_header.GetSize();

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
		   gcc_unused const struct tag_handler *handler,
		   gcc_unused void *handler_ctx)
{
	DsdiffMetaData metadata;
	DsdiffChunkHeader chunk_header;

	/* First check for DFF metadata */
	if (!dsdiff_read_metadata(nullptr, is, &metadata, &chunk_header))
		return false;

	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, metadata.sample_rate / 8,
				       SampleFormat::DSD,
				       metadata.channels, IgnoreError()))
		/* refuse to parse files which we cannot play anyway */
		return false;

	/* calculate song time and add as tag */
	unsigned songtime = ((metadata.chunk_size / metadata.channels) * 8) /
			    metadata.sample_rate;
	tag_handler_invoke_duration(handler, handler_ctx, songtime);

	/* Read additional metadata and created tags if available */
	dsdiff_read_metadata_extra(nullptr, is, &metadata, &chunk_header,
				   handler, handler_ctx);

	return true;
}

static const char *const dsdiff_suffixes[] = {
	"dff",
	nullptr
};

static const char *const dsdiff_mime_types[] = {
	"application/x-dff",
	nullptr
};

const struct decoder_plugin dsdiff_decoder_plugin = {
	"dsdiff",
	dsdiff_init,
	nullptr,
	dsdiff_stream_decode,
	nullptr,
	nullptr,
	dsdiff_scan_stream,
	nullptr,
	dsdiff_suffixes,
	dsdiff_mime_types,
};
