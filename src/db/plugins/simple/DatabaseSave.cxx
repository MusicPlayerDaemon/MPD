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
#include "DatabaseSave.hxx"
#include "db/DatabaseLock.hxx"
#include "db/DatabaseError.hxx"
#include "Directory.hxx"
#include "DirectorySave.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "fs/io/TextFile.hxx"
#include "tag/Tag.hxx"
#include "tag/TagSettings.h"
#include "fs/Charset.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <string.h>
#include <stdlib.h>

#define DIRECTORY_INFO_BEGIN "info_begin"
#define DIRECTORY_INFO_END "info_end"
#define DB_FORMAT_PREFIX "format: "
#define DIRECTORY_MPD_VERSION "mpd_version: "
#define DIRECTORY_FS_CHARSET "fs_charset: "
#define DB_TAG_PREFIX "tag: "

static constexpr unsigned DB_FORMAT = 2;

/**
 * The oldest database format understood by this MPD version.
 */
static constexpr unsigned OLDEST_DB_FORMAT = 1;

void
db_save_internal(BufferedOutputStream &os, const Directory &music_root)
{
	os.Format("%s\n", DIRECTORY_INFO_BEGIN);
	os.Format(DB_FORMAT_PREFIX "%u\n", DB_FORMAT);
	os.Format("%s%s\n", DIRECTORY_MPD_VERSION, VERSION);
	os.Format("%s%s\n", DIRECTORY_FS_CHARSET, GetFSCharset());

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (!ignore_tag_items[i])
			os.Format(DB_TAG_PREFIX "%s\n", tag_item_names[i]);

	os.Format("%s\n", DIRECTORY_INFO_END);

	directory_save(os, music_root);
}

bool
db_load_internal(TextFile &file, Directory &music_root, Error &error)
{
	char *line;
	unsigned format = 0;
	bool found_charset = false, found_version = false;
	bool success;
	bool tags[TAG_NUM_OF_ITEM_TYPES];

	/* get initial info */
	line = file.ReadLine();
	if (line == nullptr || strcmp(DIRECTORY_INFO_BEGIN, line) != 0) {
		error.Set(db_domain, "Database corrupted");
		return false;
	}

	memset(tags, false, sizeof(tags));

	while ((line = file.ReadLine()) != nullptr &&
	       strcmp(line, DIRECTORY_INFO_END) != 0) {
		if (StringStartsWith(line, DB_FORMAT_PREFIX)) {
			format = atoi(line + sizeof(DB_FORMAT_PREFIX) - 1);
		} else if (StringStartsWith(line, DIRECTORY_MPD_VERSION)) {
			if (found_version) {
				error.Set(db_domain, "Duplicate version line");
				return false;
			}

			found_version = true;
		} else if (StringStartsWith(line, DIRECTORY_FS_CHARSET)) {
			const char *new_charset;

			if (found_charset) {
				error.Set(db_domain, "Duplicate charset line");
				return false;
			}

			found_charset = true;

			new_charset = line + sizeof(DIRECTORY_FS_CHARSET) - 1;
			const char *const old_charset = GetFSCharset();
			if (*old_charset != 0
			    && strcmp(new_charset, old_charset) != 0) {
				error.Format(db_domain,
					     "Existing database has charset "
					     "\"%s\" instead of \"%s\"; "
					     "discarding database file",
					     new_charset, old_charset);
				return false;
			}
		} else if (StringStartsWith(line, DB_TAG_PREFIX)) {
			const char *name = line + sizeof(DB_TAG_PREFIX) - 1;
			TagType tag = tag_name_parse(name);
			if (tag == TAG_NUM_OF_ITEM_TYPES) {
				error.Format(db_domain,
					     "Unrecognized tag '%s', "
					     "discarding database file",
					     name);
				return false;
			}

			tags[tag] = true;
		} else {
			error.Format(db_domain, "Malformed line: %s", line);
			return false;
		}
	}

	if (format < OLDEST_DB_FORMAT || format > DB_FORMAT) {
		error.Set(db_domain,
			  "Database format mismatch, "
			  "discarding database file");
		return false;
	}

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		if (!ignore_tag_items[i] && !tags[i]) {
			error.Set(db_domain,
				  "Tag list mismatch, "
				  "discarding database file");
			return false;
		}
	}

	LogDebug(db_domain, "reading DB");

	db_lock();
	success = directory_load(file, music_root, error);
	db_unlock();

	return success;
}
