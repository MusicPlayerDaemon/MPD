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

#include "HybridDsdDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "util/ByteOrder.hxx"
#include "util/Domain.hxx"
#include "util/WritableBuffer.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "Log.hxx"

#include <string.h>

static constexpr Domain hybrid_dsd_domain("hybrid_dsd");

namespace {

bool
InitHybridDsdDecoder(const ConfigBlock &block)
{
	/* this plugin is disabled by default because for people
	   without a DSD DAC, the PCM (=ALAC) part of the file is
	   better */
	if (block.GetBlockParam("enabled") == nullptr) {
		LogDebug(hybrid_dsd_domain,
			 "The Hybrid DSD decoder is disabled because it was not explicitly enabled");
		return false;
	}

	return true;
}

/**
 * This exception gets thrown by FindHybridDsdData() to indicate a
 * file format error.
 */
struct UnsupportedFile {};

struct Mp4ChunkHeader {
	uint32_t size;
	char type[4];
};

void
ReadFull(DecoderClient &client, InputStream &input,
	 WritableBuffer<uint8_t> dest)
{
	while (!dest.empty()) {
		size_t nbytes = client.Read(input, dest.data, dest.size);
		if (nbytes == 0)
			throw UnsupportedFile();

		dest.skip_front(nbytes);
	}
}

void
ReadFull(DecoderClient &client, InputStream &input, WritableBuffer<void> dest)
{
	ReadFull(client, input, WritableBuffer<uint8_t>::FromVoid(dest));
}

template<typename T>
T
ReadFullT(DecoderClient &client, InputStream &input)
{
	T dest;
	ReadFull(client, input, WritableBuffer<void>(&dest, sizeof(dest)));
	return dest;
}

Mp4ChunkHeader
ReadHeader(DecoderClient &client, InputStream &input)
{
	return ReadFullT<Mp4ChunkHeader>(client, input);
}

uint32_t
ReadBE32(DecoderClient &client, InputStream &input)
{
	return FromBE32(ReadFullT<uint32_t>(client, input));
}

} /* anonymous namespace */

static std::pair<AudioFormat, offset_type>
FindHybridDsdData(DecoderClient &client, InputStream &input)
{
	auto audio_format = AudioFormat::Undefined();
	bool found_version = false;

	while (true) {
		auto header = ReadHeader(client, input);
		const size_t header_size = FromBE32(header.size);
		if (header_size < sizeof(header))
			throw UnsupportedFile();

		size_t remaining = header_size - sizeof(header);
		if (memcmp(header.type, "bphv", 4) == 0) {
			/* version; this plugin knows only version
			   1 */
			if (remaining != 4 || ReadBE32(client, input) != 1)
				throw UnsupportedFile();
			remaining -= 4;

			found_version = true;
		} else if (memcmp(header.type, "bphc", 4) == 0) {
			/* channel count */
			if (remaining != 4)
				throw UnsupportedFile();

			auto channels = ReadBE32(client, input);
			remaining -= 4;

			if (!audio_valid_channel_count(channels))
				throw UnsupportedFile();

			audio_format.channels = channels;
		} else if (memcmp(header.type, "bphr", 4) == 0) {
			/* (bit) sample rate */

			if (remaining != 4)
				throw UnsupportedFile();

			auto sample_rate = ReadBE32(client, input) / 8;
			remaining -= 4;

			if (!audio_valid_sample_rate(sample_rate))
				throw UnsupportedFile();

			audio_format.sample_rate = sample_rate;
		} else if (memcmp(header.type, "bphf", 4) == 0) {
			/* format: 0 = plain DSD; 1 = DST compressed
			   (only plain DSD is understood by this
			   plugin) */
			if (remaining != 4 || ReadBE32(client, input) != 0)
				throw UnsupportedFile();
			remaining -= 4;

			audio_format.format = SampleFormat::DSD;
		} else if (memcmp(header.type, "bphd", 4) == 0) {
			/* the actual DSD data */
			if (!found_version || !audio_format.IsValid())
				throw UnsupportedFile();

			return {audio_format, remaining};
		}

		/* skip this chunk payload */
		input.LockSkip(remaining);
	}
}

static void
HybridDsdDecode(DecoderClient &client, InputStream &input)
{
	if (!input.CheapSeeking())
		/* probe only if seeking is cheap, i.e. not for HTTP
		   streams */
		return;

	uint64_t total_frames;
	size_t frame_size;
	unsigned kbit_rate;

	try {
		auto result = FindHybridDsdData(client, input);
		auto duration = result.first.SizeToTime<SignedSongTime>(result.second);
		client.Ready(result.first, true, duration);
		frame_size = result.first.GetFrameSize();
		kbit_rate = frame_size * result.first.sample_rate /
			(1024U / 8U);
		total_frames = result.second / frame_size;
	} catch (UnsupportedFile) {
		/* not a Hybrid-DSD file; let the next decoder plugin
		   (e.g. FFmpeg) handle it */
		return;
	}

	const offset_type start_offset = input.GetOffset();
	offset_type remaining_bytes = total_frames * frame_size;

	StaticFifoBuffer<uint8_t, 16384> buffer;

	auto cmd = client.GetCommand();
	while (remaining_bytes > 0) {
		uint64_t seek_frame;

		switch (cmd) {
		case DecoderCommand::NONE:
		case DecoderCommand::START:
			break;

		case DecoderCommand::STOP:
			return;

		case DecoderCommand::SEEK:
			seek_frame = client.GetSeekFrame();
			if (seek_frame >= total_frames) {
				/* seeking past the end */
				client.CommandFinished();
				return;
			}

			try {
				input.LockSeek(start_offset + seek_frame * frame_size);
				remaining_bytes = (total_frames - seek_frame) * frame_size;
				buffer.Clear();
				client.CommandFinished();
			} catch (...) {
				LogError(std::current_exception());
				client.SeekError();
			}

			cmd = DecoderCommand::NONE;
			break;
		}

		/* fill the buffer */
		auto w = buffer.Write();
		if (!w.empty()) {
			if (remaining_bytes < (1<<30ULL) &&
			    w.size > size_t(remaining_bytes))
				w.size = remaining_bytes;

			const size_t nbytes = client.Read(input,
							  w.data, w.size);
			if (nbytes == 0)
				return;

			remaining_bytes -= nbytes;
			buffer.Append(nbytes);
		}

		/* submit the buffer to our client */
		auto r = buffer.Read();
		auto n_frames = r.size / frame_size;
		if (n_frames > 0) {
			cmd = client.SubmitData(input, r.data,
						n_frames * frame_size,
						kbit_rate);
			buffer.Consume(n_frames * frame_size);
		}
	}
}

static const char *const hybrid_dsd_suffixes[] = {
	"m4a",
	nullptr
};

constexpr DecoderPlugin hybrid_dsd_decoder_plugin =
	/* no scan method here; the FFmpeg plugin will do that for us,
	   and we only do the decoding */
	DecoderPlugin("hybrid_dsd", HybridDsdDecode, nullptr)
	.WithInit(InitHybridDsdDecoder)
	.WithSuffixes(hybrid_dsd_suffixes);
