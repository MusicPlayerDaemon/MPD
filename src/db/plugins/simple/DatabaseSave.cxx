// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DatabaseSave.hxx"
#include "db/DatabaseLock.hxx"
#include "DirectorySave.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/LineReader.hxx"
#include "tag/Names.hxx"
#include "tag/ParseName.hxx"
#include "tag/Settings.hxx"
#include "fs/Charset.hxx"
#include "util/StringCompare.hxx"
#include "Version.h"

#include <fmt/format.h>

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
	os.Write(DIRECTORY_INFO_BEGIN "\n");
	os.Fmt(FMT_STRING(DB_FORMAT_PREFIX "{}\n"), DB_FORMAT);
	os.Write(DIRECTORY_MPD_VERSION VERSION "\n");
	os.Fmt(FMT_STRING(DIRECTORY_FS_CHARSET "{}\n"), GetFSCharset());

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (IsTagEnabled(i))
			os.Fmt(FMT_STRING(DB_TAG_PREFIX "{}\n"),
			       tag_item_names[i]);

	os.Write(DIRECTORY_INFO_END "\n");

	directory_save(os, music_root);
}

void
db_load_internal(LineReader &file, Directory &music_root)
{
	char *line;
	unsigned format = 0;
	bool found_charset = false, found_version = false;
	bool tags[TAG_NUM_OF_ITEM_TYPES];

	/* get initial info */
	line = file.ReadLine();
	if (line == nullptr || strcmp(DIRECTORY_INFO_BEGIN, line) != 0)
		throw std::runtime_error("Database corrupted");

	memset(tags, false, sizeof(tags));

	while ((line = file.ReadLine()) != nullptr &&
	       strcmp(line, DIRECTORY_INFO_END) != 0) {
		const char *p;

		if ((p = StringAfterPrefix(line, DB_FORMAT_PREFIX))) {
			format = atoi(p);
		} else if (StringStartsWith(line, DIRECTORY_MPD_VERSION)) {
			if (found_version)
				throw std::runtime_error("Duplicate version line");

			found_version = true;
		} else if ((p = StringAfterPrefix(line, DIRECTORY_FS_CHARSET))) {
			if (found_charset)
				throw std::runtime_error("Duplicate charset line");

			found_charset = true;

			const char *new_charset = p;
			const char *const old_charset = GetFSCharset();
			if (*old_charset != 0
			    && strcmp(new_charset, old_charset) != 0)
				throw FmtRuntimeError("Existing database has charset "
						      "\"{}\" instead of \"{}\"; "
						      "discarding database file",
						      new_charset, old_charset);
		} else if ((p = StringAfterPrefix(line, DB_TAG_PREFIX))) {
			const char *name = p;
			TagType tag = tag_name_parse(name);
			if (tag == TAG_NUM_OF_ITEM_TYPES)
				throw FmtRuntimeError("Unrecognized tag '{}', "
						      "discarding database file",
						      name);

			tags[tag] = true;
		} else {
			throw FmtRuntimeError("Malformed line: {}", line);
		}
	}

	if (format < OLDEST_DB_FORMAT || format > DB_FORMAT)
		throw std::runtime_error("Database format mismatch, "
					 "discarding database file");

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (IsTagEnabled(i) && !tags[i])
			throw std::runtime_error("Tag list mismatch, "
						 "discarding database file");

	const ScopeDatabaseLock protect;
	directory_load(file, music_root);
}
