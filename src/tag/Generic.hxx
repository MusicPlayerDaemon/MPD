// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_GENERIC_HXX
#define MPD_TAG_GENERIC_HXX

class TagHandler;
class InputStream;
class Path;

/**
 * Attempts to scan APE or ID3 tags from the specified stream.  The
 * stream does not need to be rewound.
 *
 * Throws on error.
 */
bool
ScanGenericTags(InputStream &is, TagHandler &handler);

/**
 * Attempts to scan APE or ID3 tags from the specified file.
 *
 * Throws on error.
 */
bool
ScanGenericTags(Path path, TagHandler &handler);

#endif
