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

#include "config.h"
#include "FileSystem.hxx"
#include "AllocatedPath.hxx"
#include "Limits.hxx"

#include <errno.h>

AllocatedPath
ReadLink(Path path)
{
#ifdef WIN32
	(void)path;
	errno = EINVAL;
	return AllocatedPath::Null();
#else
	char buffer[MPD_PATH_MAX];
	ssize_t size = readlink(path.c_str(), buffer, MPD_PATH_MAX);
	if (size < 0)
		return AllocatedPath::Null();
	if (size_t(size) >= MPD_PATH_MAX) {
		errno = ENOMEM;
		return AllocatedPath::Null();
	}
	buffer[size] = '\0';
	return AllocatedPath::FromFS(buffer);
#endif
}
