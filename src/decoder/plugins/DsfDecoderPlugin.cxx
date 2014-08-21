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
 * This plugin decodes DSDIFF data (SACD) embedded in DSF files.
 *
 * The DSF code was created using the specification found here:
 * http://dsd-guide.com/sonys-dsf-file-format-spec
 *
 * All functions common to both DSD decoders have been moved to dsdlib
 */

#include "config.h"
#include "DsfDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "util/bit_reverse.h"
#include "util/Error.hxx"
#include "system/ByteOrder.hxx"
#include "DsdLib.hxx"
#include "tag/TagHandler.hxx"
#include "Log.hxx"

#include <string.h>

static constexpr unsigned DSF_BLOCK_SIZE = 4096;

struct DsfMetaData {
	unsigned sample_rate, channels;
	bool bitreverse;
	offset_type chunk_size;
#ifdef HAVE_ID3TAG
	offset_type id3_offset;
#endif
};

struct DsfHeader {
	/** DSF header id: "DSD " */
	DsdId id;
	/** DSD chunk size, including id = 28 */
	DsdUint64 size;
	/** total file size */
	DsdUint64 fsize;
	/** pointer to id3v2 metadata, should be at the end of the file */
	DsdUint64 pmeta;
};

/** DSF file fmt chunk */
struct DsfFmtChunk {
	/** id: "fmt " */
	DsdId id;
	/** fmt chunk size, including id, normally 52 */
	DsdUint64 size;
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
	DsdUint64 scnt;
	/** block size per channel = 4096 */
	uint32_t block_size;
	/** reserved, should be all zero */
	uint32_t reserved;
};

struct DsfDataChunk {
	DsdId id;
	/** "data" chunk size, includes header (id+size) */
	DsdUint64 size;
};

/**
 * Read and parse all needed metadata chunks for DSF files.
 */
static bool
dsf_read_metadata(Decoder *decoder, InputStream &is,
		  DsfMetaData *metadata)
{
	DsfHeader dsf_header;
	if (!decoder_read_full(decoder, is, &dsf_header, sizeof(dsf_header)) ||
	    !dsf_header.id.Equals("DSD "))
		return false;

	const offset_type chunk_size = dsf_header.size.Read();
	if (sizeof(dsf_header) != chunk_size)
		return false;

#ifdef HAVE_ID3TAG
	const offset_type metadata_offset = dsf_header.pmeta.Read();
#endif

	/* read the 'fmt ' chunk of the DSF file */
	DsfFmtChunk dsf_fmt_chunk;
	if (!decoder_read_full(decoder, is,
			       &dsf_fmt_chunk, sizeof(dsf_fmt_chunk)) ||
	    !dsf_fmt_chunk.id.Equals("fmt "))
		return false;

	const uint64_t fmt_chunk_size = dsf_fmt_chunk.size.Read();
	if (fmt_chunk_size != sizeof(dsf_fmt_chunk))
		return false;

	uint32_t samplefreq = FromLE32(dsf_fmt_chunk.sample_freq);

	/* for now, only support version 1 of the standard, DSD raw stereo
	   files with a sample freq of 2822400 or 5644800 Hz */

	if (dsf_fmt_chunk.version != 1 || dsf_fmt_chunk.formatid != 0
	    || dsf_fmt_chunk.channeltype != 2
	    || dsf_fmt_chunk.channelnum != 2
	    || (!dsdlib_valid_freq(samplefreq)))
		return false;

	uint32_t chblksize = FromLE32(dsf_fmt_chunk.block_size);
	/* according to the spec block size should always be 4096 */
	if (chblksize != DSF_BLOCK_SIZE)
		return false;

	/* read the 'data' chunk of the DSF file */
	DsfDataChunk data_chunk;
	if (!decoder_read_full(decoder, is, &data_chunk, sizeof(data_chunk)) ||
	    !data_chunk.id.Equals("data"))
		return false;

	/* data size of DSF files are padded to multiple of 4096,
	   we use the actual data size as chunk size */

	offset_type data_size = data_chunk.size.Read();
	if (data_size < sizeof(data_chunk))
		return false;

	data_size -= sizeof(data_chunk);

	/* data_size cannot be bigger or equal to total file size */
	if (is.KnownSize()) {
		const offset_type size = is.GetSize();
		if (data_size >= size)
			return false;
	}

	/* use the sample count from the DSF header as the upper
	   bound, because some DSF files contain junk at the end of
	   the "data" chunk */
	const uint64_t samplecnt = dsf_fmt_chunk.scnt.Read();
	const offset_type playable_size = samplecnt * 2 / 8;
	if (data_size > playable_size)
		data_size = playable_size;

	metadata->chunk_size = data_size;
	metadata->channels = (unsigned) dsf_fmt_chunk.channelnum;
	metadata->sample_rate = samplefreq;
#ifdef HAVE_ID3TAG
	metadata->id3_offset = metadata_offset;
#endif
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
dsf_to_pcm_order(uint8_t *dest, size_t nrbytes)
{
	uint8_t scratch[DSF_BLOCK_SIZE * 2];
	assert(nrbytes <= sizeof(scratch));

	for (size_t i = 0, j = 0; i < nrbytes; i += 2) {
		scratch[i] = *(dest+j);
		j++;
	}

	for (size_t i = 1, j = 0; i < nrbytes; i += 2) {
		scratch[i] = *(dest + DSF_BLOCK_SIZE + j);
		j++;
	}

	memcpy(dest, scratch, nrbytes);
}

/**
 * Decode one complete DSF 'data' chunk i.e. a complete song
 */
static bool
dsf_decode_chunk(Decoder &decoder, InputStream &is,
		 unsigned channels, unsigned sample_rate,
		 offset_type chunk_size,
		 bool bitreverse)
{
	uint8_t buffer[DSF_BLOCK_SIZE * 2];

	const size_t sample_size = sizeof(buffer[0]);
	const size_t frame_size = channels * sample_size;
	const unsigned buffer_frames = sizeof(buffer) / frame_size;
	const unsigned buffer_samples = buffer_frames * frame_size;
	const size_t buffer_size = buffer_samples * sample_size;

	while (chunk_size >= frame_size) {
		/* see how much aligned data from the remaining chunk
		   fits into the local buffer */
		size_t now_size = buffer_size;
		if (chunk_size < now_size) {
			unsigned now_frames = chunk_size / frame_size;
			now_size = now_frames * frame_size;
		}

		if (!decoder_read_full(&decoder, is, buffer, now_size))
			return false;

		const size_t nbytes = now_size;
		chunk_size -= nbytes;

		if (bitreverse)
			bit_reverse_buffer(buffer, buffer + nbytes);

		dsf_to_pcm_order(buffer, nbytes);

		const auto cmd = decoder_data(decoder, is, buffer, nbytes,
					      sample_rate / 1000);
		switch (cmd) {
		case DecoderCommand::NONE:
			break;

		case DecoderCommand::START:
		case DecoderCommand::STOP:
			return false;

		case DecoderCommand::SEEK:

			/* not implemented yet */
			decoder_seek_error(decoder);
			break;
			}
	}
	return dsdlib_skip(&decoder, is, chunk_size);
}

static void
dsf_stream_decode(Decoder &decoder, InputStream &is)
{
	/* check if it is a proper DSF file */
	DsfMetaData metadata;
	if (!dsf_read_metadata(&decoder, is, &metadata))
		return;

	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, metadata.sample_rate / 8,
				       SampleFormat::DSD,
				       metadata.channels, error)) {
		LogError(error);
		return;
	}
	/* Calculate song time from DSD chunk size and sample frequency */
	offset_type chunk_size = metadata.chunk_size;
	float songtime = ((chunk_size / metadata.channels) * 8) /
			 (float) metadata.sample_rate;

	/* success: file was recognized */
	decoder_initialized(decoder, audio_format, false, songtime);

	if (!dsf_decode_chunk(decoder, is, metadata.channels,
			      metadata.sample_rate,
			      chunk_size,
			      metadata.bitreverse))
		return;
}

static bool
dsf_scan_stream(InputStream &is,
		gcc_unused const struct tag_handler *handler,
		gcc_unused void *handler_ctx)
{
	/* check DSF metadata */
	DsfMetaData metadata;
	if (!dsf_read_metadata(nullptr, is, &metadata))
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

#ifdef HAVE_ID3TAG
	/* Add available tags from the ID3 tag */
	dsdlib_tag_id3(is, handler, handler_ctx, metadata.id3_offset);
#endif
	return true;
}

static const char *const dsf_suffixes[] = {
	"dsf",
	nullptr
};

static const char *const dsf_mime_types[] = {
	"application/x-dsf",
	nullptr
};

const struct DecoderPlugin dsf_decoder_plugin = {
	"dsf",
	nullptr,
	nullptr,
	dsf_stream_decode,
	nullptr,
	nullptr,
	dsf_scan_stream,
	nullptr,
	dsf_suffixes,
	dsf_mime_types,
};
