// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
