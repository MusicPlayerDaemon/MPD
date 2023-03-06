// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ARCHIVE_HXX
#define MPD_TAG_ARCHIVE_HXX

class ArchiveFile;
class TagHandler;
class TagBuilder;

/**
 * Scan the tags of a song file inside an archive.  Invokes matching
 * decoder plugins, but does not invoke the special "APE" and "ID3"
 * scanners.
 *
 * @return true if the file was recognized (even if no metadata was
 * found)
 */
bool
tag_archive_scan(ArchiveFile &archive, const char *path_utf8,
		 TagHandler &handler) noexcept;

/**
 * Scan the tags of a song file inside an archive.  Invokes matching
 * decoder plugins, and falls back to generic scanners (APE and ID3)
 * if no tags were found (but the file was recognized).
 *
 * @return true if the file was recognized (even if no metadata was
 * found)
 */
bool
tag_archive_scan(ArchiveFile &archive, const char *path_utf8,
		 TagBuilder &builder) noexcept;

#endif
