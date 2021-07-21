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

#include "Base.hxx"

#include <cassert>

#include <string.h>

static char nfs_base_server[64];
static char nfs_base_export_name[256];
static size_t nfs_base_export_name_length;

void
nfs_set_base(const char *server, const char *export_name) noexcept
{
	assert(server != nullptr);
	assert(export_name != nullptr);

	const size_t server_length = strlen(server);
	const size_t export_name_length = strlen(export_name);

	if (server_length >= sizeof(nfs_base_server) ||
	    export_name_length > sizeof(nfs_base_export_name))
		return;

	memcpy(nfs_base_server, server, server_length + 1);
	memcpy(nfs_base_export_name, export_name, export_name_length);
	nfs_base_export_name_length = export_name_length;
}

const char *
nfs_check_base(const char *server, const char *path) noexcept
{
	assert(server != nullptr);
	assert(path != nullptr);

	return strcmp(nfs_base_server, server) == 0 &&
	    memcmp(nfs_base_export_name, path,
		   nfs_base_export_name_length) == 0 &&
		(path[nfs_base_export_name_length] == 0 ||
		 path[nfs_base_export_name_length] == '/')
		? path + nfs_base_export_name_length
		: nullptr;
}
