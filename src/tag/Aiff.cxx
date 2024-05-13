// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Aiff.hxx"
#include "input/InputStream.hxx"
#include "util/ByteOrder.hxx"
#include "util/SpanCast.hxx"

#include <cstdint>
#include <limits>
#include <stdexcept>

#include <string.h>

struct aiff_header {
	char id[4];
	uint32_t size;
	char format[4];
};

struct aiff_chunk_header {
	char id[4];
	uint32_t size;
};

size_t
aiff_seek_id3(InputStream &is, std::unique_lock<Mutex> &lock)
{
	/* seek to the beginning and read the AIFF header */

	is.Rewind(lock);

	aiff_header header;
	is.ReadFull(lock, ReferenceAsWritableBytes(header));
	if (memcmp(header.id, "FORM", 4) != 0 ||
	    (is.KnownSize() && FromBE32(header.size) > is.GetSize()) ||
	    (memcmp(header.format, "AIFF", 4) != 0 &&
	     memcmp(header.format, "AIFC", 4) != 0))
		throw std::runtime_error("Not an AIFF file");

	while (true) {
		/* read the chunk header */

		aiff_chunk_header chunk;
		is.ReadFull(lock, ReferenceAsWritableBytes(chunk));

		size_t size = FromBE32(chunk.size);
		if (size > size_t(std::numeric_limits<int>::max()))
			/* too dangerous, bail out: possible integer
			   underflow when casting to off_t */
			throw std::runtime_error("AIFF chunk is too large");

		if (memcmp(chunk.id, "ID3 ", 4) == 0)
			/* found it! */
			return size;

		if (size % 2 != 0)
			/* pad byte */
			++size;

		is.Skip(lock, size);
	}
}
