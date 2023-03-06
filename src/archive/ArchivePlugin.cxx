// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ArchivePlugin.hxx"
#include "ArchiveFile.hxx"
#include "fs/Path.hxx"

#include <cassert>

std::unique_ptr<ArchiveFile>
archive_file_open(const ArchivePlugin *plugin, Path path)
{
	assert(plugin != nullptr);
	assert(plugin->open != nullptr);
	assert(!path.IsNull());

	return plugin->open(path);
}
