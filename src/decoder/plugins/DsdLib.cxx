// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* \file
 *
 * This file contains functions used by the DSF and DSDIFF decoders.
 *
 */

#include "config.h"
#include "DsdLib.hxx"

#ifdef ENABLE_ID3TAG
#include "../DecoderAPI.hxx"
#include "tag/Id3Limits.hxx"
#include "tag/Id3Parse.hxx"
#include "tag/Id3Scan.hxx"
#include "input/InputStream.hxx"
#include "util/AllocatedArray.hxx"
#endif

#include <stdlib.h>

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
bool
dsdlib_tag_id3(DecoderClient *client, InputStream &is,
	       TagHandler &handler, offset_type tagoffset)
{
	if (tagoffset == 0 || !is.KnownSize())
		return false;

	/* Prevent broken files causing problems */
	const auto size = is.GetSize();
	if (tagoffset >= size)
		return false;

	const auto count64 = size - tagoffset;
	if (count64 < 10 || count64 > MAX_ID3_TAG_SIZE)
		return false;

	if (!decoder_seek(client, is, tagoffset))
		return false;

	const id3_length_t count = count64;

	AllocatedArray<std::byte> id3_buf{count};

	if (!decoder_read_full(client, is, id3_buf))
		return false;

	const auto id3_tag = id3_tag_parse(id3_buf);
	id3_buf = nullptr;
	if (id3_tag == nullptr)
		return false;

	scan_id3_tag(id3_tag.get(), handler);
	return true;
}
#endif
