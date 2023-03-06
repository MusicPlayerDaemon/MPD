// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ARCHIVE_PLUGIN_HXX
#define MPD_ARCHIVE_PLUGIN_HXX

#include <memory>

class ArchiveFile;
class Path;

struct ArchivePlugin {
	const char *name;

	/**
	 * optional, set this to nullptr if the archive plugin doesn't
	 * have/need one this must false if there is an error and
	 * true otherwise
	 */
	bool (*init)();

	/**
	 * optional, set this to nullptr if the archive plugin doesn't
	 * have/need one
	 */
	void (*finish)();

	/**
	 * tryes to open archive file and associates handle with archive
	 * returns pointer to handle used is all operations with this archive
	 *
	 * Throws std::runtime_error on error.
	 */
	std::unique_ptr<ArchiveFile> (*open)(Path path_fs);

	/**
	 * suffixes handled by this plugin.
	 * last element in these arrays must always be a nullptr
	 */
	const char *const*suffixes;
};

std::unique_ptr<ArchiveFile>
archive_file_open(const ArchivePlugin *plugin, Path path);

#endif
