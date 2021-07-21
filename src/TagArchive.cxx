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

#include "TagArchive.hxx"
#include "TagStream.hxx"
#include "archive/ArchiveFile.hxx"
#include "input/InputStream.hxx"

bool
tag_archive_scan(ArchiveFile &archive, const char *path_utf8,
		 TagHandler &handler) noexcept
try {
	Mutex mutex;

	auto is = archive.OpenStream(path_utf8, mutex);
	if (!is)
		return false;

	return tag_stream_scan(*is, handler);
} catch (...) {
	return false;
}

bool
tag_archive_scan(ArchiveFile &archive, const char *path_utf8,
		 TagBuilder &builder) noexcept
try {
	Mutex mutex;

	auto is = archive.OpenStream(path_utf8, mutex);
	return is && tag_stream_scan(*is, builder);
} catch (...) {
	return false;
}
