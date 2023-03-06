// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderBuffer.hxx"
#include "DecoderAPI.hxx"

bool
DecoderBuffer::Fill()
{
	auto w = buffer.Write();
	if (w.empty())
		/* buffer is full */
		return false;

	size_t nbytes = decoder_read(client, is,
				     w.data(), w.size());
	if (nbytes == 0)
		/* end of file, I/O error or decoder command
		   received */
		return false;

	buffer.Append(nbytes);
	return true;
}

std::span<const std::byte>
DecoderBuffer::Need(size_t min_size)
{
	while (true) {
		const auto r = Read();
		if (r.size() >= min_size)
			return r;

		if (!Fill())
			return {};
	}
}

bool
DecoderBuffer::Skip(size_t nbytes)
{
	const auto r = buffer.Read();
	if (r.size() >= nbytes) {
		buffer.Consume(nbytes);
		return true;
	}

	buffer.Clear();
	nbytes -= r.size();

	return decoder_skip(client, is, nbytes);
}
