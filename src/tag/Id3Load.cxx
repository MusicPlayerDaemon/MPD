// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Id3Load.hxx"
#include "Id3Parse.hxx"
#include "RiffId3.hxx"
#include "Aiff.hxx"
#include "input/InputStream.hxx"

#include <id3tag.h>

#include <algorithm>

static constexpr size_t ID3V1_SIZE = 128;

[[gnu::pure]]
static inline bool
tag_is_id3v1(struct id3_tag *tag) noexcept
{
	return (id3_tag_options(tag, 0, 0) & ID3_TAG_OPTION_ID3V1) != 0;
}

static long
get_id3v2_footer_size(InputStream &is, std::unique_lock<Mutex> &lock,
		      offset_type offset)
try {
	id3_byte_t buf[ID3_TAG_QUERYSIZE];
	is.Seek(lock, offset);
	is.ReadFull(lock, std::as_writable_bytes(std::span{buf}));

	return id3_tag_query(buf, sizeof(buf));
} catch (...) {
	return 0;
}

static UniqueId3Tag
ReadId3Tag(InputStream &is, std::unique_lock<Mutex> &lock)
try {
	std::byte query_buffer[ID3_TAG_QUERYSIZE];
	is.ReadFull(lock, query_buffer);

	/* Look for a tag header */
	long query = id3_tag_query(reinterpret_cast<const id3_byte_t *>(query_buffer),
				   sizeof(query_buffer));
	if (query <= 0)
		return nullptr;

	const std::size_t tag_size = static_cast<std::size_t>(query);

	/* Found a tag.  Allocate a buffer and read it in. */
	if (tag_size <= sizeof(query_buffer))
		/* we have enough data already */
		return id3_tag_parse(std::span{query_buffer}.first(tag_size));

	auto tag_buffer = std::make_unique_for_overwrite<std::byte[]>(tag_size);

	/* copy the start of the tag we already have to the allocated
	   buffer */
	std::byte *end = std::copy_n(query_buffer, sizeof(query_buffer),
				     tag_buffer.get());

	/* now read the remaining bytes */
	const size_t remaining = tag_size - sizeof(query_buffer);
	is.ReadFull(lock, std::span{end, remaining});

	return id3_tag_parse(std::span{tag_buffer.get(), tag_size});
} catch (...) {
	return nullptr;
}

static UniqueId3Tag
ReadId3Tag(InputStream &is, std::unique_lock<Mutex> &lock, offset_type offset)
try {
	is.Seek(lock, offset);

	return ReadId3Tag(is, lock);
} catch (...) {
	return nullptr;
}

static UniqueId3Tag
ReadId3v1Tag(InputStream &is, std::unique_lock<Mutex> &lock)
try {
	std::byte buffer[ID3V1_SIZE];
	is.ReadFull(lock, buffer);
	return id3_tag_parse(buffer);
} catch (...) {
	return nullptr;
}

static UniqueId3Tag
ReadId3v1Tag(InputStream &is, std::unique_lock<Mutex> &lock,
	     offset_type offset)
try {
	is.Seek(lock, offset);
	return ReadId3v1Tag(is, lock);
} catch (...) {
	return nullptr;
}

static UniqueId3Tag
tag_id3_find_from_beginning(InputStream &is, std::unique_lock<Mutex> &lock)
try {
	auto tag = ReadId3Tag(is, lock);
	if (!tag) {
		return nullptr;
	} else if (tag_is_id3v1(tag.get())) {
		/* id3v1 tags don't belong here */
		return nullptr;
	}

	/* We have an id3v2 tag, so let's look for SEEK frames */
	id3_frame *frame;
	while ((frame = id3_tag_findframe(tag.get(), "SEEK", 0))) {
		/* Found a SEEK frame, get it's value */
		int seek = id3_field_getint(id3_frame_field(frame, 0));
		if (seek < 0)
			break;

		/* Get the tag specified by the SEEK frame */
		auto seektag = ReadId3Tag(is, lock, is.GetOffset() + seek);
		if (!seektag || tag_is_id3v1(seektag.get()))
			break;

		/* Replace the old tag with the new one */
		tag = std::move(seektag);
	}

	return tag;
} catch (...) {
	return nullptr;
}

static UniqueId3Tag
tag_id3_find_from_end(InputStream &is, std::unique_lock<Mutex> &lock)
try {
	if (!is.KnownSize() || !is.CheapSeeking())
		return nullptr;

	const offset_type size = is.GetSize();
	if (size < ID3V1_SIZE)
		return nullptr;

	offset_type offset = size - ID3V1_SIZE;

	/* Get an id3v1 tag from the end of file for later use */
	auto v1tag = ReadId3v1Tag(is, lock, offset);
	if (!v1tag)
		offset = size;

	/* Get the id3v2 tag size from the footer (located before v1tag) */
	if (offset < ID3_TAG_QUERYSIZE)
		return v1tag;

	long tag_offset =
		get_id3v2_footer_size(is, lock, offset - ID3_TAG_QUERYSIZE);
	if (tag_offset >= 0)
		return v1tag;

	offset_type tag_size = -tag_offset;
	if (tag_size > offset)
		return v1tag;

	/* Get the tag which the footer belongs to */
	auto tag = ReadId3Tag(is, lock, offset - tag_size);
	if (!tag)
		return v1tag;

	/* We have an id3v2 tag, so ditch v1tag */
	return tag;
} catch (...) {
	return nullptr;
}

static UniqueId3Tag
tag_id3_riff_aiff_load(InputStream &is, std::unique_lock<Mutex> &lock)
try {
	size_t size;
	try {
		size = riff_seek_id3(is, lock);
	} catch (...) {
		size = aiff_seek_id3(is, lock);
	}

	if (size == 0)
		return nullptr;
	if (size > 4 * 1024 * 1024)
		/* too large, don't allocate so much memory */
		return nullptr;

	auto buffer = std::make_unique_for_overwrite<std::byte[]>(size);
	is.ReadFull(lock, std::span{buffer.get(), size});
	return id3_tag_parse(std::span{buffer.get(), size});
} catch (...) {
	return nullptr;
}

UniqueId3Tag
tag_id3_load(InputStream &is)
try {
	std::unique_lock lock{is.mutex};

	auto tag = tag_id3_find_from_beginning(is, lock);
	if (tag == nullptr && is.CheapSeeking()) {
		tag = tag_id3_riff_aiff_load(is, lock);
		if (tag == nullptr)
			tag = tag_id3_find_from_end(is, lock);
	}

	return tag;
} catch (...) {
	return nullptr;
}
