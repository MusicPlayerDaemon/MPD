// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_SAVE_HXX
#define MPD_DATABASE_SAVE_HXX

struct Directory;
class BufferedOutputStream;
class LineReader;

void
db_save_internal(BufferedOutputStream &os, const Directory &root);

/**
 * Throws #std::runtime_error on error.
 *
 * @param ignore_config_mismatches if true, then configuration
 * mismatches (e.g. enabled tags or filesystem charset) are ignored
 */
void
db_load_internal(LineReader &file, Directory &root,
		 bool ignore_config_mismatches=false);

#endif
