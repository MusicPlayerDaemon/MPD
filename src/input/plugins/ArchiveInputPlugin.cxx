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

#include "ArchiveInputPlugin.hxx"
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#include "archive/ArchiveFile.hxx"
#include "../InputStream.hxx"
#include "fs/LookupFile.hxx"
#include "fs/Path.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

static constexpr Domain input_domain("input");

InputStreamPtr
OpenArchiveInputStream(Path path, Mutex &mutex)
{
	const ArchivePlugin *arplug;

	ArchiveLookupResult l;
	try {
		l = LookupFile(path);
		if (l.archive.IsNull()) {
			return nullptr;
		}
	} catch (...) {
		FmtDebug(input_domain,
			 "not an archive, lookup '{}' failed: {}",
			 path, std::current_exception());
		return nullptr;
	}

	const char *suffix = l.archive.GetSuffix();
	if (suffix == nullptr)
		return nullptr;

	//check which archive plugin to use (by ext)
	arplug = archive_plugin_from_suffix(suffix);
	if (!arplug) {
		return nullptr;
	}

	return archive_file_open(arplug, l.archive)
		->OpenStream(l.inside.c_str(), mutex);
}
