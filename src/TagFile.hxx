// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_FILE_HXX
#define MPD_TAG_FILE_HXX

struct AudioFormat;
class Path;
class TagHandler;
class TagBuilder;

/**
 * Scan the tags of a song file.  Invokes matching decoder plugins,
 * but does not fall back to generic scanners (APE and ID3) if no tags
 * were found (but the file was recognized).
 *
 * Throws on error.
 *
 * @return true if the file was recognized (even if no metadata was
 * found)
 */
bool
ScanFileTagsNoGeneric(Path path, TagHandler &handler);

/**
 * Scan the tags of a song file.  Invokes matching decoder plugins,
 * and falls back to generic scanners (APE and ID3) if no tags were
 * found (but the file was recognized).
 *
 * Throws on error.
 *
 * @return true if the file was recognized (even if no metadata was
 * found)
 */
bool
ScanFileTagsWithGeneric(Path path, TagBuilder &builder,
			AudioFormat *audio_format=nullptr);

#endif
