/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "ArchiveLookup.hxx"
#include "ArchiveList.hxx"
#include "ArchivePlugin.hxx"
#include "ArchiveFile.hxx"
#include "InputPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>

static constexpr Domain archive_domain("archive");

/**
 * select correct archive plugin to handle the input stream
 * may allow stacking of archive plugins. for example for handling
 * tar.gz a gzip handler opens file (through inputfile stream)
 * then it opens a tar handler and sets gzip inputstream as
 * parent_stream so tar plugin fetches file data from gzip
 * plugin and gzip fetches file from disk
 */
static struct input_stream *
input_archive_open(const char *pathname,
		   Mutex &mutex, Cond &cond,
		   Error &error)
{
	const struct archive_plugin *arplug;
	char *archive, *filename, *suffix, *pname;
	struct input_stream *is;

	if (!g_path_is_absolute(pathname))
		return NULL;

	pname = g_strdup(pathname);
	// archive_lookup will modify pname when true is returned
	if (!archive_lookup(pname, &archive, &filename, &suffix)) {
		FormatDebug(archive_domain,
			    "not an archive, lookup %s failed", pname);
		g_free(pname);
		return NULL;
	}

	//check which archive plugin to use (by ext)
	arplug = archive_plugin_from_suffix(suffix);
	if (!arplug) {
		FormatWarning(archive_domain,
			      "can't handle archive %s", archive);
		g_free(pname);
		return NULL;
	}

	auto file = archive_file_open(arplug, archive, error);
	if (file == NULL) {
		g_free(pname);
		return NULL;
	}

	//setup fileops
	is = file->OpenStream(filename, mutex, cond, error);
	g_free(pname);
	file->Close();

	return is;
}

const struct input_plugin input_plugin_archive = {
	"archive",
	nullptr,
	nullptr,
	input_archive_open,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};
