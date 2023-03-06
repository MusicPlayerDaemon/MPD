// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Buffer.hxx"

void *
PcmBuffer::Get(size_t new_size) noexcept
{
	if (new_size == 0)
		/* never return nullptr, because nullptr would be
		   assumed to be an error condition */
		new_size = 1;

	return buffer.Get(new_size);
}
