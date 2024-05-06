// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Base.hxx"

#include <algorithm> // for std::copy()
#include <array>
#include <cassert>

#include <string.h>

static std::array<char, 64> nfs_base_server;
static std::array<char, 256> nfs_base_export_name;
static size_t nfs_base_export_name_length;

void
nfs_set_base(std::string_view server, std::string_view export_name) noexcept
{
	if (server.size() >= nfs_base_server.size() ||
	    export_name.size() > nfs_base_export_name.size())
		return;

	*std::copy(server.begin(), server.end(),
		   nfs_base_server.begin()) = '\0';
	std::copy(export_name.begin(), export_name.end(),
		  nfs_base_export_name.begin());
	nfs_base_export_name_length = export_name.size();
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
