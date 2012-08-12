/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#ifndef MPD_CLIENT_FILE_H
#define MPD_CLIENT_FILE_H

#include <glib.h>
#include <stdbool.h>

struct client;

/**
 * Is this client allowed to use the specified local file?
 *
 * Note that this function is vulnerable to timing/symlink attacks.
 * We cannot fix this as long as there are plugins that open a file by
 * its name, and not by file descriptor / callbacks.
 *
 * @param path_fs the absolute path name in filesystem encoding
 * @return true if access is allowed
 */
bool
client_allow_file(const struct client *client, const char *path_fs,
		  GError **error_r);

#endif
