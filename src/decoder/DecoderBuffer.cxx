// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderBuffer.hxx"
#include "DecoderAPI.hxx"
#include "input/InputStream.hxx"

offset_type
DecoderBuffer::GetOffset() const noexcept
{
	return is.GetOffset() - GetAvailable();
}

bool
DecoderBuffer::Fill() noexcept
{
	auto w = buffer.Write();
	if (w.empty())
		/* buffer is full */
		return false;

	size_t nbytes = decoder_read(client, is, w);
	if (nbytes == 0)
		/* end of file, I/O error or decoder command
		   received */
		return false;

	buffer.Append(nbytes);
	return true;
}

std::span<const std::byte>
DecoderBuffer::Need(size_t min_size) noexcept
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
DecoderBuffer::Skip(size_t nbytes) noexcept
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
