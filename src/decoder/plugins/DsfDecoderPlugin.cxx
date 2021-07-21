/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "pcm/CheckAudioFormat.hxx"
#include "util/BitReverse.hxx"
#include "util/ByteOrder.hxx"
#include "DsdLib.hxx"
#include "tag/Handler.hxx"

#include <string.h>

static constexpr unsigned DSF_BLOCK_SIZE = 4096;

struct DsfMetaData {
	unsigned sample_rate, channels;
	bool bitreverse;
	offset_type n_blocks;
#ifdef ENABLE_ID3TAG
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
dsf_read_metadata(DecoderClient *client, InputStream &is,
		  DsfMetaData *metadata)
{
	DsfHeader dsf_header;
	if (!decoder_read_full(client, is, &dsf_header, sizeof(dsf_header)) ||
	    !dsf_header.id.Equals("DSD "))
		return false;

	const offset_type chunk_size = dsf_header.size.Read();
	if (sizeof(dsf_header) != chunk_size)
		return false;

#ifdef ENABLE_ID3TAG
	const offset_type metadata_offset = dsf_header.pmeta.Read();
#endif

	/* read the 'fmt ' chunk of the DSF file */
	DsfFmtChunk dsf_fmt_chunk;
	if (!decoder_read_full(client, is,
			       &dsf_fmt_chunk, sizeof(dsf_fmt_chunk)) ||
	    !dsf_fmt_chunk.id.Equals("fmt "))
		return false;

	const uint64_t fmt_chunk_size = dsf_fmt_chunk.size.Read();
	if (fmt_chunk_size != sizeof(dsf_fmt_chunk))
		return false;

	uint32_t samplefreq = FromLE32(dsf_fmt_chunk.sample_freq);
	const unsigned channels = FromLE32(dsf_fmt_chunk.channelnum);

	/* for now, only support version 1 of the standard, DSD raw stereo
	   files with a sample freq of 2822400 or 5644800 Hz */

	if (FromLE32(dsf_fmt_chunk.version) != 1 ||
	    FromLE32(dsf_fmt_chunk.formatid) != 0 ||
	    !audio_valid_channel_count(channels) ||
	    !dsdlib_valid_freq(samplefreq))
		return false;

	uint32_t chblksize = FromLE32(dsf_fmt_chunk.block_size);
	/* according to the spec block size should always be 4096 */
	if (chblksize != DSF_BLOCK_SIZE)
		return false;

	/* read the 'data' chunk of the DSF file */
	DsfDataChunk data_chunk;
	if (!decoder_read_full(client, is, &data_chunk, sizeof(data_chunk)) ||
	    !data_chunk.id.Equals("data"))
		return false;

	/* data size of DSF files are padded to multiple of 4096,
	   we use the actual data size as chunk size */

	offset_type data_size = data_chunk.size.Read();
	if (data_size < sizeof(data_chunk))
		return false;

	data_size -= sizeof(data_chunk);

	/* data_size cannot be bigger or equal to total file size */
	if (is.KnownSize() && data_size > is.GetRest())
		return false;

	/* use the sample count from the DSF header as the upper
	   bound, because some DSF files contain junk at the end of
	   the "data" chunk */
	const uint64_t samplecnt = dsf_fmt_chunk.scnt.Read();
	const offset_type playable_size = samplecnt * channels / 8;
	if (data_size > playable_size)
		data_size = playable_size;

	const size_t block_size = channels * DSF_BLOCK_SIZE;
	metadata->n_blocks = data_size / block_size;
	metadata->channels = channels;
	metadata->sample_rate = samplefreq;
#ifdef ENABLE_ID3TAG
	metadata->id3_offset = metadata_offset;
#endif
	/* check bits per sample format, determine if bitreverse is needed */
	metadata->bitreverse = FromLE32(dsf_fmt_chunk.bitssample) == 1;
	return true;
}

static void
bit_reverse_buffer(uint8_t *p, uint8_t *end)
{
	for (; p < end; ++p)
		*p = bit_reverse(*p);
}

static void
InterleaveDsfBlockMono(uint8_t *gcc_restrict dest,
		       const uint8_t *gcc_restrict src)
{
	memcpy(dest, src, DSF_BLOCK_SIZE);
}

/**
 * DSF data is build up of alternating 4096 blocks of DSD samples for left and
 * right. Convert the buffer holding 1 block of 4096 DSD left samples and 1
 * block of 4096 DSD right samples to 8k of samples in normal PCM left/right
 * order.
 */
static void
InterleaveDsfBlockStereo(uint8_t *gcc_restrict dest,
			 const uint8_t *gcc_restrict src)
{
	for (size_t i = 0; i < DSF_BLOCK_SIZE; ++i) {
		dest[2 * i] = src[i];
		dest[2 * i + 1] = src[DSF_BLOCK_SIZE + i];
	}
}

static void
InterleaveDsfBlockChannel(uint8_t *gcc_restrict dest,
			  const uint8_t *gcc_restrict src,
			  unsigned channels)
{
	for (size_t i = 0; i < DSF_BLOCK_SIZE; ++i, dest += channels, ++src)
		*dest = *src;
}

static void
InterleaveDsfBlockGeneric(uint8_t *gcc_restrict dest,
			  const uint8_t *gcc_restrict src,
			  unsigned channels)
{
	for (unsigned c = 0; c < channels; ++c, ++dest, src += DSF_BLOCK_SIZE)
		InterleaveDsfBlockChannel(dest, src, channels);
}

static void
InterleaveDsfBlock(uint8_t *gcc_restrict dest, const uint8_t *gcc_restrict src,
		   unsigned channels)
{
	if (channels == 1)
		InterleaveDsfBlockMono(dest, src);
	else if (channels == 2)
		InterleaveDsfBlockStereo(dest, src);
	else
		InterleaveDsfBlockGeneric(dest, src, channels);
}

static offset_type
FrameToBlock(uint64_t frame)
{
	return frame / DSF_BLOCK_SIZE;
}

/**
 * Decode one complete DSF 'data' chunk i.e. a complete song
 */
static bool
dsf_decode_chunk(DecoderClient &client, InputStream &is,
		 unsigned channels, unsigned sample_rate,
		 offset_type n_blocks,
		 bool bitreverse)
{
	const unsigned kbit_rate = channels * sample_rate / 1000;
	const size_t block_size = channels * DSF_BLOCK_SIZE;
	const offset_type start_offset = is.GetOffset();

	auto cmd = client.GetCommand();
	for (offset_type i = 0; i < n_blocks && cmd != DecoderCommand::STOP;) {
		if (cmd == DecoderCommand::SEEK) {
			uint64_t frame = client.GetSeekFrame();
			offset_type block = FrameToBlock(frame);
			if (block >= n_blocks) {
				client.CommandFinished();
				break;
			}

			offset_type offset =
				start_offset + block * block_size;
			if (dsdlib_skip_to(&client, is, offset)) {
				client.CommandFinished();
				i = block;
			} else
				client.SeekError();
		}

		/* worst-case buffer size */
		uint8_t buffer[MAX_CHANNELS * DSF_BLOCK_SIZE];
		if (!decoder_read_full(&client, is, buffer, block_size))
			return false;

		if (bitreverse)
			bit_reverse_buffer(buffer, buffer + block_size);

		uint8_t interleaved_buffer[MAX_CHANNELS * DSF_BLOCK_SIZE];
		InterleaveDsfBlock(interleaved_buffer, buffer, channels);

		cmd = client.SubmitData(is,
					interleaved_buffer, block_size,
					kbit_rate);
		++i;
	}

	return true;
}

static void
dsf_stream_decode(DecoderClient &client, InputStream &is)
{
	/* check if it is a proper DSF file */
	DsfMetaData metadata;
	if (!dsf_read_metadata(&client, is, &metadata))
		return;

	auto audio_format = CheckAudioFormat(metadata.sample_rate / 8,
					     SampleFormat::DSD,
					     metadata.channels);

	/* Calculate song time from DSD chunk size and sample frequency */
	const auto n_blocks = metadata.n_blocks;
	auto songtime = SongTime::FromScale<uint64_t>(n_blocks * DSF_BLOCK_SIZE,
						      audio_format.sample_rate);

	/* success: file was recognized */
	client.Ready(audio_format, is.IsSeekable(), songtime);

	dsf_decode_chunk(client, is, metadata.channels,
			 metadata.sample_rate,
			 n_blocks,
			 metadata.bitreverse);
}

static bool
dsf_scan_stream(InputStream &is, TagHandler &handler)
{
	/* check DSF metadata */
	DsfMetaData metadata;
	if (!dsf_read_metadata(nullptr, is, &metadata))
		return false;

	const auto sample_rate = metadata.sample_rate / 8;
	if (!audio_valid_sample_rate(sample_rate))
		return false;

	/* calculate song time and add as tag */
	const auto n_blocks = metadata.n_blocks;
	auto songtime = SongTime::FromScale<uint64_t>(n_blocks * DSF_BLOCK_SIZE,
						      sample_rate);
	handler.OnDuration(songtime);

#ifdef ENABLE_ID3TAG
	/* Add available tags from the ID3 tag */
	dsdlib_tag_id3(is, handler, metadata.id3_offset);
#endif
	return true;
}

static const char *const dsf_suffixes[] = {
	"dsf",
	nullptr
};

static const char *const dsf_mime_types[] = {
	"application/x-dsf",
	"audio/x-dsf",
	"audio/x-dsd",
	nullptr
};

constexpr DecoderPlugin dsf_decoder_plugin =
	DecoderPlugin("dsf", dsf_stream_decode, dsf_scan_stream)
	.WithSuffixes(dsf_suffixes)
	.WithMimeTypes(dsf_mime_types);
