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

#include "client_file.h"
#include "client.h"
#include "ack.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

bool
client_allow_file(const struct client *client, const char *path_fs,
		  GError **error_r)
{
#ifdef WIN32
	(void)client;
	(void)path_fs;

	g_set_error(error_r, ack_quark(), ACK_ERROR_PERMISSION,
		    "Access denied");
	return false;
#else
	const int uid = client_get_uid(client);
	if (uid >= 0 && (uid_t)uid == geteuid())
		/* always allow access if user runs his own MPD
		   instance */
		return true;

	if (uid <= 0) {
		/* unauthenticated client */
		g_set_error(error_r, ack_quark(), ACK_ERROR_PERMISSION,
			    "Access denied");
		return false;
	}

	struct stat st;
	if (stat(path_fs, &st) < 0) {
		g_set_error(error_r, g_file_error_quark(), errno,
			    "%s", g_strerror(errno));
		return false;
	}

	if (st.st_uid != (uid_t)uid && (st.st_mode & 0444) != 0444) {
		/* client is not owner */
		g_set_error(error_r, ack_quark(), ACK_ERROR_PERMISSION,
			    "Access denied");
		return false;
	}

	return true;
#endif
}
