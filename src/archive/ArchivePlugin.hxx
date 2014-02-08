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

#ifndef MPD_ARCHIVE_PLUGIN_HXX
#define MPD_ARCHIVE_PLUGIN_HXX

class ArchiveFile;
class Path;
class Error;

struct ArchivePlugin {
	const char *name;

	/**
	 * optional, set this to nullptr if the archive plugin doesn't
	 * have/need one this must false if there is an error and
	 * true otherwise
	 */
	bool (*init)(void);

	/**
	 * optional, set this to nullptr if the archive plugin doesn't
	 * have/need one
	 */
	void (*finish)(void);

	/**
	 * tryes to open archive file and associates handle with archive
	 * returns pointer to handle used is all operations with this archive
	 * or nullptr when opening fails
	 */
	ArchiveFile *(*open)(Path path_fs, Error &error);

	/**
	 * suffixes handled by this plugin.
	 * last element in these arrays must always be a nullptr
	 */
	const char *const*suffixes;
};

ArchiveFile *
archive_file_open(const ArchivePlugin *plugin, Path path,
		  Error &error);

#endif
