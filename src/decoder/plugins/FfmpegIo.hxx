// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

extern "C" {
#include "libavformat/avio.h"
}

#include <cstddef>
#include <cstdint>
#include <span>

class DecoderClient;
class InputStream;

struct AvioStream {
	DecoderClient *const client;
	InputStream &input;

	AVIOContext *io = nullptr;

	AvioStream(DecoderClient *_client, InputStream &_input) noexcept
		:client(_client), input(_input) {}

	~AvioStream() noexcept;

	/**
	 * @return true on success
	 */
	bool Open() noexcept;

private:
	int Read(std::span<std::byte> dest) noexcept;
	int64_t Seek(int64_t pos, int whence) noexcept;

	static int _Read(void *opaque, uint8_t *buf, int size) noexcept;
	static int64_t _Seek(void *opaque, int64_t pos, int whence) noexcept;
};
