/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_ARCHIVE_FILE_HXX
#define MPD_ARCHIVE_FILE_HXX

class Mutex;
class Cond;
class Error;
struct ArchivePlugin;
class ArchiveVisitor;
class InputStream;

class ArchiveFile {
public:
	const ArchivePlugin &plugin;

	ArchiveFile(const ArchivePlugin &_plugin)
		:plugin(_plugin) {}

protected:
	/**
	 * Use Close() instead of delete.
	 */
	~ArchiveFile() {}

public:
	virtual void Close() = 0;

	/**
	 * Visit all entries inside this archive.
	 */
	virtual void Visit(ArchiveVisitor &visitor) = 0;

	/**
	 * Opens an InputStream of a file within the archive.
	 *
	 * @param path the path within the archive
	 */
	virtual InputStream *OpenStream(const char *path,
					Mutex &mutex, Cond &cond,
					Error &error) = 0;
};

#endif
