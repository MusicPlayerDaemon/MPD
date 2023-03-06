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
 */
void
db_load_internal(LineReader &file, Directory &root);

#endif
