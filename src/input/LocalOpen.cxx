/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "LocalOpen.hxx"
#include "InputStream.hxx"
#include "plugins/FileInputPlugin.hxx"

#ifdef ENABLE_ARCHIVE
#include "plugins/ArchiveInputPlugin.hxx"
#endif

#include "fs/Path.hxx"
#include "system/Error.hxx"

#include <assert.h>

#ifdef ENABLE_ARCHIVE
#include <errno.h>
#endif

InputStreamPtr
OpenLocalInputStream(Path path, Mutex &mutex, Cond &cond)
{
	InputStreamPtr is;

#ifdef ENABLE_ARCHIVE
	try {
#endif
		is = OpenFileInputStream(path, mutex, cond);
#ifdef ENABLE_ARCHIVE
	} catch (const std::system_error &e) {
		if (IsPathNotFound(e)) {
			/* ENOTDIR means this may be a path inside an archive
			   file */
			is = OpenArchiveInputStream(path, mutex, cond);
			if (!is)
				throw;
		} else
			throw;
	}
#endif

	assert(is == nullptr || is->IsReady());

	return is;
}
