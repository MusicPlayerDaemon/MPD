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
#include "ArchiveInputPlugin.hxx"
#include "archive/ArchiveDomain.hxx"
#include "archive/ArchiveLookup.hxx"
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#include "archive/ArchiveFile.hxx"
#include "../InputPlugin.hxx"
#include "fs/Traits.hxx"
#include "fs/Path.hxx"
#include "util/Alloc.hxx"
#include "Log.hxx"

#include <stdlib.h>

InputStream *
OpenArchiveInputStream(Path path, Mutex &mutex, Cond &cond, Error &error)
{
	const ArchivePlugin *arplug;
	InputStream *is;

	char *pname = strdup(path.c_str());
	// archive_lookup will modify pname when true is returned
	const char *archive, *filename, *suffix;
	if (!archive_lookup(pname, &archive, &filename, &suffix)) {
		FormatDebug(archive_domain,
			    "not an archive, lookup %s failed", pname);
		free(pname);
		return nullptr;
	}

	//check which archive plugin to use (by ext)
	arplug = archive_plugin_from_suffix(suffix);
	if (!arplug) {
		FormatWarning(archive_domain,
			      "can't handle archive %s", archive);
		free(pname);
		return nullptr;
	}

	auto file = archive_file_open(arplug, Path::FromFS(archive), error);
	if (file == nullptr) {
		free(pname);
		return nullptr;
	}

	//setup fileops
	is = file->OpenStream(filename, mutex, cond, error);
	free(pname);
	file->Close();

	return is;
}

static InputStream *
input_archive_open(gcc_unused const char *filename,
		gcc_unused Mutex &mutex, gcc_unused Cond &cond,
		gcc_unused Error &error)
{
	/* dummy method; use OpenArchiveInputStream() instead */

	return nullptr;
}

const InputPlugin input_plugin_archive = {
	"archive",
	nullptr,
	nullptr,
	input_archive_open,
};
