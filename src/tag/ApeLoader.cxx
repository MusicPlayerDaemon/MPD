// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ApeLoader.hxx"
#include "input/InputStream.hxx"
#include "util/PackedLittleEndian.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

struct ApeFooter {
	unsigned char id[8];
	uint32_t version;
	uint32_t length;
	uint32_t count;
	unsigned char flags[4];
	unsigned char reserved[8];
};

bool
tag_ape_scan(InputStream &is, const ApeTagCallback& callback)
try {
	std::unique_lock lock{is.mutex};

	if (!is.KnownSize() || !is.CheapSeeking())
		return false;

	/* determine if file has an apeV2 tag */
	ApeFooter footer;
	is.Seek(lock, is.GetSize() - sizeof(footer));
	is.ReadFull(lock, ReferenceAsWritableBytes(footer));

	if (memcmp(footer.id, "APETAGEX", sizeof(footer.id)) != 0 ||
	    FromLE32(footer.version) != 2000)
		return false;

	/* find beginning of ape tag */
	size_t remaining = FromLE32(footer.length);
	if (remaining <= sizeof(footer) + 10 ||
	    /* refuse to load more than one megabyte of tag data */
	    remaining > 1024 * 1024)
		return false;

	is.Seek(lock, is.GetSize() - remaining);

	/* read tag into buffer */
	remaining -= sizeof(footer);
	assert(remaining > 10);

	auto buffer = std::make_unique<std::byte[]>(remaining);
	is.ReadFull(lock, {buffer.get(), remaining});

	/* read tags */
	unsigned n = FromLE32(footer.count);
	const char *p = (const char *)buffer.get();
	while (n-- && remaining > 10) {
		size_t size = *(const PackedLE32 *)p;
		p += 4;
		remaining -= 4;
		unsigned long flags = *(const PackedLE32 *)p;
		p += 4;
		remaining -= 4;

		/* get the key */
		const char *key = p;
		const char *key_end = (const char *)std::memchr(p, '\0', remaining);
		if (key_end == nullptr)
			break;

		p = key_end + 1;
		remaining -= p - key;

		/* get the value */
		if (remaining < size)
			break;

		if (!callback(flags, key, {p, size}))
			break;

		p += size;
		remaining -= size;
	}

	return true;
} catch (...) {
	return false;
}
