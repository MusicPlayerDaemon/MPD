/*
 * Copyright (C) 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "Id3Load.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "Riff.hxx"
#include "Aiff.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/FileSystem.hxx"

#include <id3tag.h>

#include <stdio.h>

static constexpr Domain id3_domain("id3");

gcc_pure
static inline bool
tag_is_id3v1(struct id3_tag *tag)
{
	return (id3_tag_options(tag, 0, 0) & ID3_TAG_OPTION_ID3V1) != 0;
}

static size_t
fill_buffer(void *buf, size_t size, FILE *stream, long offset, int whence)
{
	if (fseek(stream, offset, whence) != 0) return 0;
	return fread(buf, 1, size, stream);
}

static long
get_id3v2_footer_size(FILE *stream, long offset, int whence)
{
	id3_byte_t buf[ID3_TAG_QUERYSIZE];
	size_t bufsize = fill_buffer(buf, ID3_TAG_QUERYSIZE, stream, offset, whence);
	if (bufsize == 0) return 0;
	return id3_tag_query(buf, bufsize);
}

static UniqueId3Tag
tag_id3_read(FILE *stream, long offset, int whence)
{
	/* It's ok if we get less than we asked for */
	id3_byte_t query_buffer[ID3_TAG_QUERYSIZE];
	size_t query_buffer_size = fill_buffer(query_buffer, ID3_TAG_QUERYSIZE,
					       stream, offset, whence);
	if (query_buffer_size <= 0)
		return nullptr;

	/* Look for a tag header */
	long tag_size = id3_tag_query(query_buffer, query_buffer_size);
	if (tag_size <= 0) return nullptr;

	/* Found a tag.  Allocate a buffer and read it in. */
	if (size_t(tag_size) <= query_buffer_size)
		/* we have enough data already */
		return UniqueId3Tag(id3_tag_parse(query_buffer, tag_size));

	std::unique_ptr<id3_byte_t[]> tag_buffer(new id3_byte_t[tag_size]);
	int tag_buffer_size = fill_buffer(tag_buffer.get(), tag_size,
					  stream, offset, whence);
	if (tag_buffer_size < tag_size)
		return nullptr;

	return UniqueId3Tag(id3_tag_parse(tag_buffer.get(), tag_buffer_size));
}

static UniqueId3Tag
tag_id3_find_from_beginning(FILE *stream)
{
	auto tag = tag_id3_read(stream, 0, SEEK_SET);
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
		auto seektag = tag_id3_read(stream, seek, SEEK_CUR);
		if (!seektag || tag_is_id3v1(seektag.get()))
			break;

		/* Replace the old tag with the new one */
		tag = std::move(seektag);
	}

	return tag;
}

static UniqueId3Tag
tag_id3_find_from_end(FILE *stream)
{
	/* Get an id3v1 tag from the end of file for later use */
	auto v1tag = tag_id3_read(stream, -128, SEEK_END);

	/* Get the id3v2 tag size from the footer (located before v1tag) */
	int tagsize = get_id3v2_footer_size(stream, (v1tag ? -128 : 0) - 10, SEEK_END);
	if (tagsize >= 0)
		return v1tag;

	/* Get the tag which the footer belongs to */
	auto tag = tag_id3_read(stream, tagsize, SEEK_CUR);
	if (!tag)
		return v1tag;

	/* We have an id3v2 tag, so ditch v1tag */
	return tag;
}

static UniqueId3Tag
tag_id3_riff_aiff_load(FILE *file)
{
	size_t size = riff_seek_id3(file);
	if (size == 0)
		size = aiff_seek_id3(file);
	if (size == 0)
		return nullptr;

	if (size > 4 * 1024 * 1024)
		/* too large, don't allocate so much memory */
		return nullptr;

	std::unique_ptr<id3_byte_t[]> buffer(new id3_byte_t[size]);
	size_t ret = fread(buffer.get(), size, 1, file);
	if (ret != 1) {
		LogWarning(id3_domain, "Failed to read RIFF chunk");
		return nullptr;
	}

	return UniqueId3Tag(id3_tag_parse(buffer.get(), size));
}

UniqueId3Tag
tag_id3_load(Path path_fs, Error &error)
{
	FILE *file = FOpen(path_fs, PATH_LITERAL("rb"));
	if (file == nullptr) {
		error.FormatErrno("Failed to open file %s",
				  NarrowPath(path_fs).c_str());
		return nullptr;
	}

	auto tag = tag_id3_find_from_beginning(file);
	if (tag == nullptr) {
		tag = tag_id3_riff_aiff_load(file);
		if (tag == nullptr)
			tag = tag_id3_find_from_end(file);
	}

	fclose(file);
	return tag;
}
