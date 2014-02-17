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
#include "TextFile.hxx"
#include "util/Alloc.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

TextFile::TextFile(Path path_fs)
	:file(FOpen(path_fs, FOpenMode::ReadText)),
	 buffer((char *)xalloc(step)), capacity(step), length(0) {}

TextFile::~TextFile()
{
	free(buffer);

	if (file != nullptr)
		fclose(file);
}

char *
TextFile::ReadLine()
{
	assert(file != nullptr);

	while (true) {
		if (length >= capacity) {
			if (capacity >= max_length)
				/* too large already - bail out */
				return nullptr;

			capacity <<= 1;
			char *new_buffer = (char *)realloc(buffer, capacity);
			if (new_buffer == nullptr)
				/* out of memory - bail out */
				return nullptr;
		}

		char *p = fgets(buffer + length, capacity - length, file);
		if (p == nullptr) {
			if (length == 0 || ferror(file))
				return nullptr;
			break;
		}

		length += strlen(buffer + length);
		if (buffer[length - 1] == '\n')
			break;
	}

	/* remove the newline characters */
	if (buffer[length - 1] == '\n')
		--length;
	if (buffer[length - 1] == '\r')
		--length;

	buffer[length] = 0;
	length = 0;
	return buffer;
}
