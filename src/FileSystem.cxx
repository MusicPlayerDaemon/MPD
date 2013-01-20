/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "FileSystem.hxx"

#include <errno.h>

bool ReadLink(const Path &path, Path &result)
{
#ifdef WIN32
	(void)path;
	result = Path::Null();
	errno = EINVAL;
	return false;
#else
	char buffer[MPD_PATH_MAX];
	ssize_t size = readlink(path.c_str(), buffer, MPD_PATH_MAX);
	int orig_errno = errno;
	if (size < 0) {
		result = Path::Null();
		errno = orig_errno;
		return false;
	}
	if (size >= MPD_PATH_MAX) {
		result = Path::Null();
		errno = ENOMEM;
		return false;
	}
	buffer[size] = '\0';
	result = Path::FromFS(buffer);
	if (result.IsNull()) {
		errno = ENOMEM;
		return false;
	}
	errno = orig_errno;
	return true;
#endif
}
