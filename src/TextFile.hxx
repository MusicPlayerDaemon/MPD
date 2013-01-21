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

#ifndef MPD_TEXT_FILE_HXX
#define MPD_TEXT_FILE_HXX

#include "gcc.h"
#include "fs/Path.hxx"

#include <glib.h>

#include <stdio.h>

class TextFile {
	static constexpr size_t max_length = 512 * 1024;
	static constexpr size_t step = 1024;

	FILE *const file;

	GString *const buffer;

public:
	TextFile(const Path &path_fs)
		:file(fopen(path_fs.c_str(), "r")),
		 buffer(g_string_sized_new(step)) {}

	TextFile(const TextFile &other) = delete;

	~TextFile() {
		if (file != nullptr)
			fclose(file);

		g_string_free(buffer, true);
	}

	bool HasFailed() const {
		return gcc_unlikely(file == nullptr);
	}

	/**
	 * Reads a line from the input file, and strips trailing
	 * space.  There is a reasonable maximum line length, only to
	 * prevent denial of service.
	 *
	 * @param file the source file, opened in text mode
	 * @param buffer an allocator for the buffer
	 * @return a pointer to the line, or NULL on end-of-file or error
	 */
	char *ReadLine();
};

#endif
