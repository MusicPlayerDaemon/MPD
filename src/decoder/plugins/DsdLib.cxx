/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "tag/TagId3.hxx"
#include "util/Error.hxx"
#include "util/Alloc.hxx"

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

bool
DsdId::Equals(const char *s) const
{
	assert(s != nullptr);
	assert(strlen(s) == sizeof(value));

	return memcmp(value, s, sizeof(value)) == 0;
}

/**
 * Skip the #input_stream to the specified offset.
 */
bool
dsdlib_skip_to(Decoder *decoder, InputStream &is,
	       offset_type offset)
{
	if (is.IsSeekable())
		return is.LockSeek(offset, IgnoreError());

	if (is.GetOffset() > offset)
		return false;

	return dsdlib_skip(decoder, is, offset - is.GetOffset());
}

/**
 * Skip some bytes from the #input_stream.
 */
bool
dsdlib_skip(Decoder *decoder, InputStream &is,
	    offset_type delta)
{
	if (delta == 0)
		return true;

	if (is.IsSeekable())
		return is.LockSeek(is.GetOffset() + delta, IgnoreError());

	if (delta > 1024 * 1024)
		/* don't skip more than one megabyte; it would be too
		   expensive */
		return false;

	return decoder_skip(decoder, is, delta);
}

bool
dsdlib_valid_freq(uint32_t samplefreq)
{
	switch (samplefreq) {
	case 2822400: /* DSD64, 64xFs, Fs = 44.100kHz */
	case 3072000: /* DSD64 with Fs = 48.000 kHz */
	case 5644800:
	case 6144000:
	case 11289600:
	case 12288000:
	case 22579200:/* DSD512 */
	case 24576000:
		return true;

	default:
		return false;
	}
}

#ifdef HAVE_ID3TAG
void
dsdlib_tag_id3(InputStream &is,
	       const struct tag_handler *handler,
	       void *handler_ctx, int64_t tagoffset)
{
	assert(tagoffset >= 0);

	if (tagoffset == 0 || !is.KnownSize())
		return;

	if (!dsdlib_skip_to(nullptr, is, tagoffset))
		return;

	/* Prevent broken files causing problems */
	const auto size = is.GetSize();
	const auto offset = is.GetOffset();
	if (offset >= size)
		return;

	const id3_length_t count = size - offset;

	if (count < 10 || count > 1024 * 1024)
		return;

	id3_byte_t *const id3_buf = new id3_byte_t[count];
	if (id3_buf == nullptr)
		return;

	if (!decoder_read_full(nullptr, is, id3_buf, count)) {
		delete[] id3_buf;
		return;
	}

	struct id3_tag *id3_tag = id3_tag_parse(id3_buf, count);
	delete[] id3_buf;
	if (id3_tag == nullptr)
		return;

	scan_id3_tag(id3_tag, handler, handler_ctx);

	id3_tag_delete(id3_tag);
	return;
}
#endif
