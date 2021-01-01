/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
