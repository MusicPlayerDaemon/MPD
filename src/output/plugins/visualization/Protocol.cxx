// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Protocol.hxx"

#include "Log.hxx"
#include "util/ByteOrder.hxx"
#include "util/Domain.hxx"

Visualization::ParseResult
Visualization::ParseClihlo(void *data,
			   size_t length,
			   ClientHello &clihlo) noexcept {

	// CLIHLO payload is 6 bytes, header & footer are five more.
	if (length < sizeof(ClientHello) + 5) {
		return ParseResult::NEED_MORE_DATA;
	}

	uint8_t *buf = (uint8_t *)data;

	uint16_t msg_type = FromBE16(*(uint16_t *)buf);
	if (msg_type != 0) {
		return ParseResult::ERROR;
	}

	buf += 2;
	uint16_t payload_len = FromBE16(*(uint16_t *)buf);
	if (payload_len != 6) {
		return ParseResult::ERROR;
	}

	buf += 2;
	clihlo.major_version = *buf++;
	clihlo.minor_version = *buf++;

	clihlo.requested_fps = FromBE16(*(uint16_t *)(buf));
	buf += 2;
	clihlo.tau = FromBE16(*(int16_t *)(buf));
	buf += 2;

	if (*buf != 0) {
		return ParseResult::ERROR;
	}

	return ParseResult::OK;
}
