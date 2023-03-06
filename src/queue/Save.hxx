// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This library saves the queue into the state file, and also loads it
 * back into memory.
 */

#pragma once

struct Queue;
class BufferedOutputStream;
class LineReader;
class SongLoader;

void
queue_save(BufferedOutputStream &os, const Queue &queue);

/**
 * Loads one song from the state file and appends it to the queue.
 *
 * Throws on error.
 */
void
queue_load_song(LineReader &file, const SongLoader &loader,
		const char *line, Queue &queue);
