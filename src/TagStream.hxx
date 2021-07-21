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

#ifndef MPD_TAG_STREAM_HXX
#define MPD_TAG_STREAM_HXX

struct AudioFormat;
class InputStream;
class TagHandler;
class TagBuilder;

/**
 * Scan the tags of an #InputStream.  Invokes matching decoder
 * plugins, but does not invoke the special "APE" and "ID3" scanners.
 *
 * Throws on I/O error.
 *
 * @return true if the file was recognized (even if no metadata was
 * found)
 */
bool
tag_stream_scan(InputStream &is, TagHandler &handler);

/**
 * Throws on I/O error.
 */
bool
tag_stream_scan(const char *uri, TagHandler &handler);

/**
 * Scan the tags of an #InputStream.  Invokes matching decoder
 * plugins, and falls back to generic scanners (APE and ID3) if no
 * tags were found (but the file was recognized).
 *
 * Throws on I/O error.
 *
 * @return true if the file was recognized (even if no metadata was
 * found)
 */
bool
tag_stream_scan(InputStream &is, TagBuilder &builder,
		AudioFormat *audio_format=nullptr);

/**
 * Throws on I/O error.
 */
bool
tag_stream_scan(const char *uri, TagBuilder &builder,
		AudioFormat *audio_format=nullptr);

#endif
