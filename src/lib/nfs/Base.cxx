// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Base.hxx"

#include <array>
#include <cassert>

#include <string.h>

static std::array<char, 64> nfs_base_server;
static std::array<char, 256> nfs_base_export_name;
static size_t nfs_base_export_name_length;

void
nfs_set_base(const char *server, const char *export_name) noexcept
{
	assert(server != nullptr);
	assert(export_name != nullptr);

	const size_t server_length = strlen(server);
	const size_t export_name_length = strlen(export_name);

	if (server_length >= nfs_base_server.size() ||
	    export_name_length > nfs_base_export_name.size())
		return;

	memcpy(nfs_base_server.data(), server, server_length + 1);
	memcpy(nfs_base_export_name.data(), export_name, export_name_length);
	nfs_base_export_name_length = export_name_length;
}

const char *
nfs_check_base(const char *server, const char *path) noexcept
{
	assert(server != nullptr);
	assert(path != nullptr);

	return strcmp(nfs_base_server.data(), server) == 0 &&
		memcmp(nfs_base_export_name.data(), path,
		       nfs_base_export_name_length) == 0 &&
		(path[nfs_base_export_name_length] == 0 ||
		 path[nfs_base_export_name_length] == '/')
		? path + nfs_base_export_name_length
		: nullptr;
}
