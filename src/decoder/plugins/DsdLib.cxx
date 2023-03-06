// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* \file
 *
 * This file contains functions used by the DSF and DSDIFF decoders.
 *
 */

#include "config.h"
#include "DsdLib.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "tag/Id3Scan.hxx"

#ifdef ENABLE_ID3TAG
#include <id3tag.h>
#endif

#include <string.h>
#include <stdlib.h>

bool
DsdId::Equals(const char *s) const noexcept
{
	assert(s != nullptr);
	assert(strlen(s) == sizeof(value));

	return memcmp(value, s, sizeof(value)) == 0;
}

/**
 * Skip the #InputStream to the specified offset.
 */
bool
dsdlib_skip_to(DecoderClient *client, InputStream &is,
	       offset_type offset)
{
	if (is.IsSeekable()) {
		try {
			is.LockSeek(offset);
		} catch (...) {
			return false;
		}
	}

	if (is.GetOffset() > offset)
		return false;

	return dsdlib_skip(client, is, offset - is.GetOffset());
}

/**
 * Skip some bytes from the #InputStream.
 */
bool
dsdlib_skip(DecoderClient *client, InputStream &is,
	    offset_type delta)
{
	if (delta == 0)
		return true;

	if (is.IsSeekable()) {
		try {
			is.LockSeek(is.GetOffset() + delta);
		} catch (...) {
			return false;
		}
	}

	if (delta > 1024 * 1024)
		/* don't skip more than one megabyte; it would be too
		   expensive */
		return false;

	return decoder_skip(client, is, delta);
}

bool
dsdlib_valid_freq(uint32_t samplefreq) noexcept
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

#ifdef ENABLE_ID3TAG
void
dsdlib_tag_id3(InputStream &is, TagHandler &handler,
	       offset_type tagoffset)
{
	if (tagoffset == 0 || !is.KnownSize())
		return;

	/* Prevent broken files causing problems */
	const auto size = is.GetSize();
	if (tagoffset >= size)
		return;

	const auto count64 = size - tagoffset;
	if (count64 < 10 || count64 > 4 * 1024 * 1024)
		return;

	if (!dsdlib_skip_to(nullptr, is, tagoffset))
		return;

	const id3_length_t count = count64;

	auto *const id3_buf = new id3_byte_t[count];
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

	scan_id3_tag(id3_tag, handler);

	id3_tag_delete(id3_tag);
}
#endif
