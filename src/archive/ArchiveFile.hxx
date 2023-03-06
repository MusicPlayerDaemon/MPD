// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ARCHIVE_FILE_HXX
#define MPD_ARCHIVE_FILE_HXX

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

class ArchiveVisitor;

class ArchiveFile {
public:
	virtual ~ArchiveFile() noexcept = default;

	/**
	 * Visit all entries inside this archive.
	 */
	virtual void Visit(ArchiveVisitor &visitor) = 0;

	/**
	 * Opens an InputStream of a file within the archive.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param path the path within the archive
	 */
	virtual InputStreamPtr OpenStream(const char *path,
					  Mutex &mutex) = 0;
};

#endif
