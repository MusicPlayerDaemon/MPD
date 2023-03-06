// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Common functions used for Ogg data streams (Ogg-Vorbis and OggFLAC)
 */

#include "OggCodec.hxx"
#include "../DecoderAPI.hxx"

#include <string.h>

enum ogg_codec
ogg_codec_detect(DecoderClient *client, InputStream &is)
{
	/* oggflac detection based on code in ogg123 and this post
	 * http://lists.xiph.org/pipermail/flac/2004-December/000393.html
	 * ogg123 trunk still doesn't have this patch as of June 2005 */
	unsigned char buf[41];
	size_t r = decoder_read(client, is, buf, sizeof(buf));
	if (r < sizeof(buf) || memcmp(buf, "OggS", 4) != 0)
		return OGG_CODEC_UNKNOWN;

	if ((memcmp(buf + 29, "FLAC", 4) == 0 &&
	     memcmp(buf + 37, "fLaC", 4) == 0) ||
	    memcmp(buf + 28, "FLAC", 4) == 0 ||
	    memcmp(buf + 28, "fLaC", 4) == 0)
		return OGG_CODEC_FLAC;

	if (memcmp(buf + 28, "Opus", 4) == 0)
		return OGG_CODEC_OPUS;

	return OGG_CODEC_VORBIS;
}
