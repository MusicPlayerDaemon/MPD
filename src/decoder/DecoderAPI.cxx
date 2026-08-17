// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "util/IntOverflow.hxx"
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
decoder_skip(DecoderClient *client, InputStream &is, offset_type delta) noexcept
{
	if (delta > 1024 && is.IsSeekable() &&
	    (delta > 1024 * 1024 || is.CheapSeeking())) {
		offset_type new_offset;
		if (AddOverflow(is.GetOffset(), delta, new_offset))
			return false;

		return decoder_seek(client, is, new_offset);
	}

	if (delta > 4 * 1024 * 1024)
		/* skipping that much would be too expensive */
		return false;

	while (delta > 0) {
		std::byte buffer[1024];

		std::span<std::byte> dest{buffer};
		if (delta < dest.size())
			dest = dest.first(delta);

		size_t nbytes = decoder_read(client, is, dest);
		if (nbytes == 0)
			return false;

		delta -= nbytes;
	}

	return true;
}

bool
decoder_seek(DecoderClient *client, InputStream &is, offset_type new_offset) noexcept
{
	if (is.IsSeekable()) {
		if (client != nullptr)
			return client->Seek(is, new_offset);

		try {
			is.LockSeek(new_offset);
			return true;
		} catch (...) {
			LogError(std::current_exception());
			return false;
		}
	}

	if (is.GetOffset() > new_offset)
		return false;

	return decoder_skip(client, is, new_offset - is.GetOffset());
}
