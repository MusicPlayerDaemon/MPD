// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DIRECTORY_SAVE_HXX
#define MPD_DIRECTORY_SAVE_HXX

struct Directory;
class LineReader;
class BufferedOutputStream;

void
directory_save(BufferedOutputStream &os, const Directory &directory);

/**
 * Throws #std::runtime_error on error.
 */
void
directory_load(LineReader &file, Directory &directory);

#endif
