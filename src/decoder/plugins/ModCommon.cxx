/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ModCommon.hxx"
#include "Log.hxx"

static constexpr size_t MOD_PREALLOC_BLOCK = 256 * 1024;
static constexpr offset_type MOD_FILE_LIMIT = 100 * 1024 * 1024;

WritableBuffer<uint8_t>
mod_loadfile(const Domain *domain, DecoderClient *client, InputStream &is)
{
	//known/unknown size, preallocate array, lets read in chunks

	const bool is_stream = !is.KnownSize();

	WritableBuffer<uint8_t> buffer;
	if (is_stream)
		buffer.size = MOD_PREALLOC_BLOCK;
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

		buffer.size = size;
	}

	buffer.data = new uint8_t[buffer.size];

	uint8_t *const end = buffer.end();
	uint8_t *p = buffer.begin();

	while (true) {
		size_t ret = decoder_read(client, is, p, end - p);
		if (ret == 0) {
			if (is.LockIsEOF())
				/* end of file */
				break;

			/* I/O error - skip this song */
			delete[] buffer.data;
			buffer.data = nullptr;
			return buffer;
		}

		p += ret;
		if (p == end) {
			if (!is_stream)
				break;

			LogWarning(*domain, "stream too large");
			delete[] buffer.data;
			buffer.data = nullptr;
			return buffer;
		}
	}

	buffer.size = p - buffer.data;
	return buffer;
}

