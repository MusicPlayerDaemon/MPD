// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "Log.hxx"

#include <cassert>

size_t
decoder_read(DecoderClient *client,
	     InputStream &is,
	     std::span<std::byte> dest) noexcept
{
	/* XXX don't allow client==nullptr */
	if (client != nullptr)
		return client->Read(is, dest);

	try {
		return is.LockRead(dest);
	} catch (...) {
		LogError(std::current_exception());
		return 0;
	}
}

size_t
decoder_read_much(DecoderClient *client, InputStream &is,
		  std::span<std::byte> dest) noexcept
{
	size_t total = 0;

	while (!dest.empty() && !is.LockIsEOF()) {
		size_t nbytes = decoder_read(client, is, dest);
		if (nbytes == 0)
			return false;

		dest = dest.subspan(nbytes);
		total += nbytes;
	}

	return total;
}

bool
decoder_read_full(DecoderClient *client, InputStream &is,
		  std::span<std::byte> dest) noexcept
{
	while (!dest.empty()) {
		size_t nbytes = decoder_read(client, is, dest);
		if (nbytes == 0)
			return false;

		dest = dest.subspan(nbytes);
	}

	return true;
}

bool
decoder_skip(DecoderClient *client, InputStream &is, size_t size) noexcept
{
	while (size > 0) {
		std::byte buffer[1024];

		size_t nbytes = decoder_read(client, is,
					     std::span{buffer, std::min(sizeof(buffer), size)});
		if (nbytes == 0)
			return false;

		size -= nbytes;
	}

	return true;
}
