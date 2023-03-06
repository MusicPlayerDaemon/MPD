// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OUTPUT_SNAPCAST_PROTOCOL_HXX
#define MPD_OUTPUT_SNAPCAST_PROTOCOL_HXX

#include "util/ByteOrder.hxx"

// see https://github.com/badaix/snapcast/blob/master/doc/binary_protocol.md

enum class SnapcastMessageType : uint16_t {
	CODEC_HEADER = 1,
	WIRE_CHUNK = 2,
	SERVER_SETTINGS = 3,
	TIME = 4,
	HELLO = 5,
	STREAM_TAGS = 6,
};

struct SnapcastTimestamp {
	PackedLE32 sec, usec;

	constexpr SnapcastTimestamp operator-(SnapcastTimestamp other) const noexcept {
		const uint32_t a_sec = sec, a_usec = usec;
		const uint32_t b_sec = other.sec, b_usec = other.usec;

		uint32_t result_sec = a_sec - b_sec;
		uint32_t result_usec = a_usec - b_usec;

		if (a_usec < b_usec) {
			--result_sec;
			result_usec += 1'000'000;
		}

		return {result_sec, result_usec};
	}
};

struct SnapcastBase {
	PackedLE16 type;
	PackedLE16 id;
	PackedLE16 refers_to;
	SnapcastTimestamp sent;
	SnapcastTimestamp received;
	PackedLE32 size;
};

static_assert(sizeof(SnapcastBase) == 26);

struct SnapcastWireChunk {
	SnapcastTimestamp timestamp;
	PackedLE32 size;
};

struct SnapcastTime {
	SnapcastTimestamp latency;
};

#endif
