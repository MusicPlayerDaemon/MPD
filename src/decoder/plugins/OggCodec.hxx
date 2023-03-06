// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Common functions used for Ogg data streams (Ogg-Vorbis and OggFLAC)
 */

#ifndef MPD_OGG_CODEC_HXX
#define MPD_OGG_CODEC_HXX

class DecoderClient;
class InputStream;

enum ogg_codec {
	OGG_CODEC_UNKNOWN,
	OGG_CODEC_VORBIS,
	OGG_CODEC_FLAC,
	OGG_CODEC_OPUS,
};

enum ogg_codec
ogg_codec_detect(DecoderClient *client, InputStream &is);

#endif /* _OGG_COMMON_H */
