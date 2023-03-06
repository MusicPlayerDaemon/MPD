// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ModCommon.hxx"
#include "Log.hxx"

static constexpr size_t MOD_PREALLOC_BLOCK = 256 * 1024;
static constexpr offset_type MOD_FILE_LIMIT = 100 * 1024 * 1024;

AllocatedArray<std::byte>
mod_loadfile(const Domain *domain, DecoderClient *client, InputStream &is)
{
	//known/unknown size, preallocate array, lets read in chunks

	const bool is_stream = !is.KnownSize();
	size_t buffer_size;

	if (is_stream) 
		buffer_size = MOD_PREALLOC_BLOCK;
	else {
		const auto size = is.GetSize();

		if (size == 0) {
			LogWarning(*domain, "file is empty");
			return nullptr;
		}

		if (size > MOD_FILE_LIMIT) {
			LogWarning(*domain, "file too large");
			return nullptr;
		}

		buffer_size = size;
	}

	auto buffer = AllocatedArray<std::byte>(buffer_size);

	std::byte *p = buffer.data();
	std::byte *const end = p + buffer.size();

	while (true) {
		size_t ret = decoder_read(client, is, p, end - p);
		if (ret == 0) {
			if (is.LockIsEOF())
				/* end of file */
				break;

			/* I/O error - skip this song */
			return buffer;
		}

		p += ret;
		if (p == end) {
			if (!is_stream)
				break;

			LogWarning(*domain, "stream too large");
			return buffer;
		}
	}
	
	buffer.SetSize(p - buffer.data());
	return buffer;
}

