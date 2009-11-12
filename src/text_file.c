/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "text_file.h"

#include <assert.h>
#include <string.h>

char *
read_text_line(FILE *file, GString *buffer)
{
	enum {
		max_length = 512 * 1024,
		step = 1024,
	};

	gsize length = 0, i;
	char *p;

	assert(file != NULL);
	assert(buffer != NULL);

	if (buffer->allocated_len < step)
		g_string_set_size(buffer, step);

	while (buffer->len < max_length) {
		p = fgets(buffer->str + length,
			  buffer->allocated_len - length, file);
		if (p == NULL) {
			if (length == 0 || ferror(file))
				return NULL;
			break;
		}

		i = strlen(buffer->str + length);
		length += i;
		if (i < step - 1 || buffer->str[length - 1] == '\n')
			break;

		g_string_set_size(buffer, length + step);
	}

	g_string_set_size(buffer, length);
	g_strchomp(buffer->str);
	return buffer->str;
}
