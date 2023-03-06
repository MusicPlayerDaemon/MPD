// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "CheckFile.hxx"
#include "Log.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "config/Domain.hxx"
#include "fs/FileInfo.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/DirectoryReader.hxx"
#include "system/Error.hxx"

void
CheckDirectoryReadable(Path path_fs)
try {
	const FileInfo fi(path_fs);
	if (!fi.IsDirectory()) {
		FmtError(config_domain, "Not a directory: {}", path_fs);
		return;
	}

#ifndef _WIN32
	try {
		const auto x = path_fs / Path::FromFS(PathTraitsFS::CURRENT_DIRECTORY);
		const FileInfo fi2(x);
	} catch (const std::system_error &e) {
		if (IsAccessDenied(e))
			FmtError(config_domain,
				 "No permission to traverse (\"execute\") directory: {}",
				 path_fs);
	}
#endif

	try {
		const DirectoryReader reader(path_fs);
	} catch (const std::system_error &e) {
		if (IsAccessDenied(e))
			FmtError(config_domain,
				 "No permission to read directory: {}",
				 path_fs);
	}
} catch (...) {
	LogError(std::current_exception());
}
