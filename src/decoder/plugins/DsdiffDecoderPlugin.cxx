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
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "util/bit_reverse.h"
#include "util/Error.hxx"
#include "system/ByteOrder.hxx"
#include "tag/TagHandler.hxx"
#include "DsdLib.hxx"
#include "Log.hxx"

struct DsdiffHeader {
	DsdId id;
	DffDsdUint64 size;
	DsdId format;
};

struct DsdiffChunkHeader {
	DsdId id;
	DffDsdUint64 size;

	/**
	 * Read the "size" attribute from the specified header, converting it
	 * to the host byte order if needed.
	 */
	constexpr
	uint64_t GetSize() const {
		return size.Read();
	}
};

/** struct for DSDIFF native Artist and Title tags */
struct dsdiff_native_tag {
	uint32_t size;
};

struct DsdiffMetaData {
	unsigned sample_rate, channels;
	bool bitreverse;
	offset_type chunk_size;
};

static bool lsbitfirst;

static bool
dsdiff_init(const config_param &param)
{
	lsbitfirst = param.GetBlockValue("lsbitfirst", false);
	return true;
}

static bool
dsdiff_read_id(Decoder *decoder, InputStream &is,
	       DsdId *id)
{
	return decoder_read_full(decoder, is, id, sizeof(*id));
}

static bool
dsdiff_read_chunk_header(Decoder *decoder, InputStream &is,
			 DsdiffChunkHeader *header)
{
	return decoder_read_full(decoder, is, header, sizeof(*header));
}

static bool
dsdiff_read_payload(Decoder *decoder, InputStream &is,
		    const DsdiffChunkHeader *header,
		    void *data, size_t length)
{
	uint64_t size = header->GetSize();
	if (size != (uint64_t)length)
		return false;

	return decoder_read_full(decoder, is, data, length);
}

/**
 * Read and parse a "SND" chunk inside "PROP".
 */
static bool
dsdiff_read_prop_snd(Decoder *decoder, InputStream &is,
		     DsdiffMetaData *metadata,
		     offset_type end_offset)
{
	DsdiffChunkHeader header;
	while (is.GetOffset() + sizeof(header) <= end_offset) {
		if (!dsdiff_read_chunk_header(decoder, is, &header))
			return false;

		offset_type chunk_end_offset = is.GetOffset()
			+ header.GetSize();
		if (chunk_end_offset > end_offset)
			return false;

		if (header.id.Equals("FS  ")) {
			uint32_t sample_rate;
			if (!dsdiff_read_payload(decoder, is, &header,
						 &sample_rate,
						 sizeof(sample_rate)))
				return false;

			metadata->sample_rate = FromBE32(sample_rate);
		} else if (header.id.Equals("CHNL")) {
			uint16_t channels;
			if (header.GetSize() < sizeof(channels) ||
			    !decoder_read_full(decoder, is,
					       &channels, sizeof(channels)) ||
			    !dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;

			metadata->channels = FromBE16(channels);
		} else if (header.id.Equals("CMPR")) {
			DsdId type;
			if (header.GetSize() < sizeof(type) ||
			    !decoder_read_full(decoder, is,
					       &type, sizeof(type)) ||
			    !dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;

			if (!type.Equals("DSD "))
				/* only uncompressed DSD audio data
				   is implemented */
				return false;
		} else {
			/* ignore unknown chunk */

			if (!dsdlib_skip_to(decoder, is, chunk_end_offset))
				return false;
		}
	}

	return is.GetOffset() == end_offset;
}

/**
 * Read and parse a "PROP" chunk.
 */
static bool
dsdiff_read_prop(Decoder *decoder, InputStream &is,
		 DsdiffMetaData *metadata,
		 const DsdiffChunkHeader *prop_header)
{
	uint64_t prop_size = prop_header->GetSize();
	const offset_type end_offset = is.GetOffset() + prop_size;

	DsdId prop_id;
	if (prop_size < sizeof(prop_id) ||
	    !dsdiff_read_id(decoder, is, &prop_id))
		return false;

	if (prop_id.Equals("SND "))
		return dsdiff_read_prop_snd(decoder, is, metadata, end_offset);
	else
		/* ignore unknown PROP chunk */
		return dsdlib_skip_to(decoder, is, end_offset);
}

static void
dsdiff_handle_native_tag(InputStream &is,
			 const struct tag_handler *handler,
			 void *handler_ctx, offset_type tagoffset,
			 TagType type)
{
	if (!dsdlib_skip_to(nullptr, is, tagoffset))
		return;

	struct dsdiff_native_tag metatag;

	if (!decoder_read_full(nullptr, is, &metatag, sizeof(metatag)))
		return;

	uint32_t length = FromBE32(metatag.size);

	/* Check and limit size of the tag to prevent a stack overflow */
	if (length == 0 || length > 60)
		return;

	char string[length];
	char *label;
	label = string;

	if (!decoder_read_full(nullptr, is, label, (size_t)length))
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
dsdiff_read_metadata_extra(Decoder *decoder, InputStream &is,
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

	/** offset for artist tag */
	offset_type artist_offset = 0;
	/** offset for title tag */
	offset_type title_offset = 0;

#ifdef HAVE_ID3TAG
	offset_type id3_offset = 0;
#endif

	/* Now process all the remaining chunk headers in the stream
	   and record their position and size */

	do {
		offset_type chunk_size = chunk_header->GetSize();

		/* DIIN chunk, is directly followed by other chunks  */
		if (chunk_header->id.Equals("DIIN"))
			chunk_size = 0;

		/* DIAR chunk - DSDIFF native tag for Artist */
		if (chunk_header->id.Equals("DIAR")) {
			chunk_size = chunk_header->GetSize();
			artist_offset = is.GetOffset();
		}

		/* DITI chunk - DSDIFF native tag for Title */
		if (chunk_header->id.Equals("DITI")) {
			chunk_size = chunk_header->GetSize();
			title_offset = is.GetOffset();
		}
#ifdef HAVE_ID3TAG
		/* 'ID3 ' chunk, offspec. Used by sacdextract */
		if (chunk_header->id.Equals("ID3 ")) {
			chunk_size = chunk_header->GetSize();
			id3_offset = is.GetOffset();
		}
#endif

		if (!dsdlib_skip(decoder, is, chunk_size))
			break;
	} while (dsdiff_read_chunk_header(decoder, is, chunk_header));

	/* done processing chunk headers, process tags if any */

#ifdef HAVE_ID3TAG
	if (id3_offset != 0) {
		/* a ID3 tag has preference over the other tags, do not process
		   other tags if we have one */
		dsdlib_tag_id3(is, handler, handler_ctx, id3_offset);
		return true;
	}
#endif

	if (artist_offset != 0)
		dsdiff_handle_native_tag(is, handler, handler_ctx,
					 artist_offset, TAG_ARTIST);

	if (title_offset != 0)
		dsdiff_handle_native_tag(is, handler, handler_ctx,
					 title_offset, TAG_TITLE);
	return true;
}

/**
 * Read and parse all metadata chunks at the beginning.  Stop when the
 * first "DSD" chunk is seen, and return its header in the
 * "chunk_header" parameter.
 */
static bool
dsdiff_read_metadata(Decoder *decoder, InputStream &is,
		     DsdiffMetaData *metadata,
		     DsdiffChunkHeader *chunk_header)
{
	DsdiffHeader header;
	if (!decoder_read_full(decoder, is, &header, sizeof(header)) ||
	    !header.id.Equals("FRM8") ||
	    !header.format.Equals("DSD "))
		return false;

	while (true) {
		if (!dsdiff_read_chunk_header(decoder, is,
					      chunk_header))
			return false;

		if (chunk_header->id.Equals("PROP")) {
			if (!dsdiff_read_prop(decoder, is, metadata,
					      chunk_header))
					return false;
		} else if (chunk_header->id.Equals("DSD ")) {
			const offset_type chunk_size = chunk_header->GetSize();
			metadata->chunk_size = chunk_size;
			return true;
		} else {
			/* ignore unknown chunk */
			const offset_type chunk_size = chunk_header->GetSize();
			const offset_type chunk_end_offset =
				is.GetOffset() + chunk_size;

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

static offset_type
FrameToOffset(uint64_t frame, unsigned channels)
{
	return frame * channels;
}

/**
 * Decode one "DSD" chunk.
 */
static bool
dsdiff_decode_chunk(Decoder &decoder, InputStream &is,
		    unsigned channels, unsigned sample_rate,
		    const offset_type total_bytes)
{
	const offset_type start_offset = is.GetOffset();

	uint8_t buffer[8192];

	const size_t sample_size = sizeof(buffer[0]);
	const size_t frame_size = channels * sample_size;
	const unsigned buffer_frames = sizeof(buffer) / frame_size;
	const size_t buffer_size = buffer_frames * frame_size;

	auto cmd = decoder_get_command(decoder);
	for (offset_type remaining_bytes = total_bytes;
	     remaining_bytes >= frame_size && cmd != DecoderCommand::STOP;) {
		if (cmd == DecoderCommand::SEEK) {
			uint64_t frame = decoder_seek_where_frame(decoder);
			offset_type offset = FrameToOffset(frame, channels);
			if (offset >= total_bytes) {
				decoder_command_finished(decoder);
				break;
			}

			if (dsdlib_skip_to(&decoder, is,
					   start_offset + offset)) {
				decoder_command_finished(decoder);
				remaining_bytes = total_bytes - offset;
			} else
				decoder_seek_error(decoder);
		}

		/* see how much aligned data from the remaining chunk
		   fits into the local buffer */
		size_t now_size = buffer_size;
		if (remaining_bytes < (offset_type)now_size) {
			unsigned now_frames = remaining_bytes / frame_size;
			now_size = now_frames * frame_size;
		}

		if (!decoder_read_full(&decoder, is, buffer, now_size))
			return false;

		const size_t nbytes = now_size;
		remaining_bytes -= nbytes;

		if (lsbitfirst)
			bit_reverse_buffer(buffer, buffer + nbytes);

		cmd = decoder_data(decoder, is, buffer, nbytes,
				   sample_rate / 1000);
	}

	return true;
}

static void
dsdiff_stream_decode(Decoder &decoder, InputStream &is)
{
	DsdiffMetaData metadata;

	DsdiffChunkHeader chunk_header;
	/* check if it is is a proper DFF file */
	if (!dsdiff_read_metadata(&decoder, is, &metadata, &chunk_header))
		return;

	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, metadata.sample_rate / 8,
				       SampleFormat::DSD,
				       metadata.channels, error)) {
		LogError(error);
		return;
	}

	/* calculate song time from DSD chunk size and sample frequency */
	offset_type chunk_size = metadata.chunk_size;

	uint64_t n_frames = chunk_size / audio_format.channels;
	auto songtime = SongTime::FromScale<uint64_t>(n_frames,
						      audio_format.sample_rate);

	/* success: file was recognized */
	decoder_initialized(decoder, audio_format, is.IsSeekable(), songtime);

	/* every iteration of the following loop decodes one "DSD"
	   chunk from a DFF file */

	dsdiff_decode_chunk(decoder, is,
			    metadata.channels,
			    metadata.sample_rate,
			    chunk_size);
}

static bool
dsdiff_scan_stream(InputStream &is,
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
	uint64_t n_frames = metadata.chunk_size / audio_format.channels;
	auto songtime = SongTime::FromScale<uint64_t>(n_frames,
						      audio_format.sample_rate);
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

const struct DecoderPlugin dsdiff_decoder_plugin = {
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
