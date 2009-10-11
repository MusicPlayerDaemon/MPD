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

#ifndef MPD_INPUT_PLUGIN_H
#define MPD_INPUT_PLUGIN_H

#include "input_stream.h"

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

struct config_param;
struct input_stream;

struct input_plugin {
	const char *name;

	/**
	 * Global initialization.  This method is called when MPD starts.
	 *
	 * @return true on success, false if the plugin should be
	 * disabled
	 */
	bool (*init)(const struct config_param *param);

	/**
	 * Global deinitialization.  Called once before MPD shuts
	 * down (only if init() has returned true).
	 */
	void (*finish)(void);

	bool (*open)(struct input_stream *is, const char *url);
	void (*close)(struct input_stream *is);

	struct tag *(*tag)(struct input_stream *is);
	int (*buffer)(struct input_stream *is);
	size_t (*read)(struct input_stream *is, void *ptr, size_t size);
	bool (*eof)(struct input_stream *is);
	bool (*seek)(struct input_stream *is, goffset offset, int whence);
};

#endif
