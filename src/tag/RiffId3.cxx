// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "RiffId3.hxx"
#include "RiffFormat.hxx"
#include "input/InputStream.hxx"
#include "util/ByteOrder.hxx"
#include "util/SpanCast.hxx"

#include <limits>
#include <stdexcept>

#include <string.h>

size_t
riff_seek_id3(InputStream &is, std::unique_lock<Mutex> &lock)
{
	/* seek to the beginning and read the RIFF header */

	is.Rewind(lock);

	RiffFileHeader header;
	is.ReadFull(lock, ReferenceAsWritableBytes(header));
	if (memcmp(header.id, "RIFF", 4) != 0 ||
	    (is.KnownSize() && FromLE32(header.size) > is.GetSize()))
		throw std::runtime_error("Not a RIFF file");

	while (true) {
		/* read the chunk header */

		RiffChunkHeader chunk;
		is.ReadFull(lock, ReferenceAsWritableBytes(chunk));

		size_t size = FromLE32(chunk.size);
		if (size > size_t(std::numeric_limits<int>::max()))
			/* too dangerous, bail out: possible integer
			   underflow when casting to off_t */
			throw std::runtime_error("RIFF chunk is too large");

		if (memcmp(chunk.id, "id3 ", 4) == 0 ||
		    memcmp(chunk.id, "ID3 ", 4) == 0)
			/* found it! */
			return size;

		if (size % 2 != 0)
			/* pad byte */
			++size;

		is.Skip(lock, size);
	}
}
