// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "Log.hxx"

#include <cassert>

size_t
decoder_read(DecoderClient *client,
	     InputStream &is,
	     void *buffer, size_t length) noexcept
{
	assert(buffer != nullptr);

	/* XXX don't allow client==nullptr */
	if (client != nullptr)
		return client->Read(is, buffer, length);

	try {
		return is.LockRead(buffer, length);
	} catch (...) {
		LogError(std::current_exception());
		return 0;
	}
}

size_t
decoder_read_much(DecoderClient *client, InputStream &is,
		  void *_buffer, size_t size) noexcept
{
	auto buffer = (uint8_t *)_buffer;

	size_t total = 0;

	while (size > 0 && !is.LockIsEOF()) {
		size_t nbytes = decoder_read(client, is, buffer, size);
		if (nbytes == 0)
			return false;

		total += nbytes;
		buffer += nbytes;
		size -= nbytes;
	}

	return total;
}

bool
decoder_read_full(DecoderClient *client, InputStream &is,
		  void *_buffer, size_t size) noexcept
{
	auto buffer = (uint8_t *)_buffer;

	while (size > 0) {
		size_t nbytes = decoder_read(client, is, buffer, size);
		if (nbytes == 0)
			return false;

		buffer += nbytes;
		size -= nbytes;
	}

	return true;
}

bool
decoder_skip(DecoderClient *client, InputStream &is, size_t size) noexcept
{
	while (size > 0) {
		char buffer[1024];
		size_t nbytes = decoder_read(client, is, buffer,
					     std::min(sizeof(buffer), size));
		if (nbytes == 0)
			return false;

		size -= nbytes;
	}

	return true;
}
