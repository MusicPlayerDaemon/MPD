/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "HybridDsdDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "system/ByteOrder.hxx"
#include "util/Domain.hxx"
#include "util/WritableBuffer.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "Log.hxx"

#include <string.h>

static constexpr Domain hybrid_dsd_domain("hybrid_dsd");

namespace {

static bool
InitHybridDsdDecoder(const ConfigBlock &block)
{
	if (block.GetBlockParam("enabled") == nullptr) {
		LogInfo(hybrid_dsd_domain,
			"The Hybrid DSD decoder is disabled because it was not explicitly enabled");
		return false;
	}

	return true;
}

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

	while (true) {
		auto header = ReadHeader(client, input);
		const size_t header_size = FromBE32(header.size);
		if (header_size < sizeof(header))
			throw UnsupportedFile();

		size_t remaining = header_size - sizeof(header);
		if (memcmp(header.type, "bphv", 4) == 0) {
			/* ? */
			if (remaining != 4 || ReadBE32(client, input) != 1)
				throw UnsupportedFile();
			remaining -= 4;

			audio_format.format = SampleFormat::DSD;
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
			/* ? */
			if (remaining != 4 || ReadBE32(client, input) != 0)
				throw UnsupportedFile();
			remaining -= 4;
		} else if (memcmp(header.type, "bphd", 4) == 0) {
			/* the actual DSD data */
			if (!audio_format.IsValid())
				throw UnsupportedFile();

			return std::make_pair(audio_format, remaining);
		}

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

	offset_type remaining_bytes;
	size_t frame_size;

	try {
		auto result = FindHybridDsdData(client, input);
		auto duration = SignedSongTime::FromS(result.second / result.first.GetTimeToSize());
		client.Ready(result.first,
			     /* TODO: implement seeking */ false,
			     duration);
		frame_size = result.first.GetFrameSize();
		remaining_bytes = result.second;
	} catch (UnsupportedFile) {
		return;
	}

	StaticFifoBuffer<uint8_t, 16384> buffer;

	auto cmd = client.GetCommand();
	while (remaining_bytes > 0) {
		switch (cmd) {
		case DecoderCommand::NONE:
		case DecoderCommand::START:
			break;

		case DecoderCommand::STOP:
			return;

		case DecoderCommand::SEEK:
			// TODO: implement seeking
			break;
		}

		auto w = buffer.Write();
		if (!w.empty()) {
			if (remaining_bytes < (1<<30ull) &&
			    w.size > size_t(remaining_bytes))
				w.size = remaining_bytes;

			const size_t nbytes = client.Read(input,
							  w.data, w.size);
			if (nbytes == 0)
				return;

			remaining_bytes -= nbytes;
			buffer.Append(nbytes);
		}

		auto r = buffer.Read();
		auto n_frames = r.size / frame_size;
		if (n_frames > 0) {
			cmd = client.SubmitData(input, r.data,
						n_frames * frame_size,
						0);
			buffer.Consume(n_frames * frame_size);
		}
	}
}

static const char *const hybrid_dsd_suffixes[] = {
	"m4a",
	nullptr
};

const struct DecoderPlugin hybrid_dsd_decoder_plugin = {
	"hybrid_dsd",
	InitHybridDsdDecoder,
	nullptr,
	HybridDsdDecode,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	hybrid_dsd_suffixes,
	nullptr,
};
