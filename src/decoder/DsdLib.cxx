/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

/* \file
 *
 * This file contains functions used by the DSF and DSDIFF decoders.
 *
 */

#include "config.h"
#include "DsdLib.hxx"
#include "DecoderAPI.hxx"
#include "InputStream.hxx"
#include "util/bit_reverse.h"
#include "TagHandler.hxx"
#include "tag/TagId3.hxx"
#include "util/Error.hxx"

#include <unistd.h>
#include <string.h>
#include <stdio.h> /* for SEEK_SET, SEEK_CUR */

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

bool
dsdlib_id_equals(const struct dsdlib_id *id, const char *s)
{
	assert(id != nullptr);
	assert(s != nullptr);
	assert(strlen(s) == sizeof(id->value));

	return memcmp(id->value, s, sizeof(id->value)) == 0;
}

bool
dsdlib_read(struct decoder *decoder, struct input_stream *is,
	    void *data, size_t length)
{
	size_t nbytes = decoder_read(decoder, is, data, length);
	return nbytes == length;
}

/**
 * Skip the #input_stream to the specified offset.
 */
bool
dsdlib_skip_to(struct decoder *decoder, struct input_stream *is,
	       goffset offset)
{
	if (is->IsSeekable())
		return is->Seek(offset, SEEK_SET, IgnoreError());

	if (is->GetOffset() > offset)
		return false;

	char buffer[8192];
	while (is->GetOffset() < offset) {
		size_t length = sizeof(buffer);
		if (offset - is->GetOffset() < (goffset)length)
			length = offset - is->GetOffset();

		size_t nbytes = decoder_read(decoder, is, buffer, length);
		if (nbytes == 0)
			return false;
	}

	assert(is->GetOffset() == offset);
	return true;
}

/**
 * Skip some bytes from the #input_stream.
 */
bool
dsdlib_skip(struct decoder *decoder, struct input_stream *is,
	    goffset delta)
{
	assert(delta >= 0);

	if (delta == 0)
		return true;

	if (is->IsSeekable())
		return is->Seek(delta, SEEK_CUR, IgnoreError());

	char buffer[8192];
	while (delta > 0) {
		size_t length = sizeof(buffer);
		if ((goffset)length > delta)
			length = delta;

		size_t nbytes = decoder_read(decoder, is, buffer, length);
		if (nbytes == 0)
			return false;

		delta -= nbytes;
	}

	return true;
}

/**
 * Add tags from ID3 tag. All tags commonly found in the ID3 tags of
 * DSF and DSDIFF files are imported
 */

#ifdef HAVE_ID3TAG
void
dsdlib_tag_id3(struct input_stream *is,
	       const struct tag_handler *handler,
	       void *handler_ctx, goffset tagoffset)
{
	assert(tagoffset >= 0);

	if (tagoffset == 0)
		return;

	if (!dsdlib_skip_to(nullptr, is, tagoffset))
		return;

	struct id3_tag *id3_tag = nullptr;
	id3_length_t count;

	/* Prevent broken files causing problems */
	const goffset size = is->GetSize();
	const goffset offset = is->GetOffset();
	if (offset >= size)
		return;

	count = size - offset;

	/* Check and limit id3 tag size to prevent a stack overflow */
	if (count == 0 || count > 4096)
		return;

	id3_byte_t dsdid3[count];
	id3_byte_t *dsdid3data;
	dsdid3data = dsdid3;

	if (!dsdlib_read(nullptr, is, dsdid3data, count))
		return;

	id3_tag = id3_tag_parse(dsdid3data, count);
	if (id3_tag == nullptr)
		return;

	scan_id3_tag(id3_tag, handler, handler_ctx);

	id3_tag_delete(id3_tag);

	return;
}
#endif
