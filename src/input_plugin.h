/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
	 * @param error_r location to store the error occurring, or
	 * NULL to ignore errors
	 * @return true on success, false if the plugin should be
	 * disabled
	 */
	bool (*init)(const struct config_param *param, GError **error_r);

	/**
	 * Global deinitialization.  Called once before MPD shuts
	 * down (only if init() has returned true).
	 */
	void (*finish)(void);

	struct input_stream *(*open)(const char *uri,
				     GMutex *mutex, GCond *cond,
				     GError **error_r);
	void (*close)(struct input_stream *is);

	/**
	 * Check for errors that may have occurred in the I/O thread.
	 * May be unimplemented for synchronous plugins.
	 *
	 * @return false on error
	 */
	bool (*check)(struct input_stream *is, GError **error_r);

	/**
	 * Update the public attributes.  Call before access.  Can be
	 * NULL if the plugin always keeps its attributes up to date.
	 */
	void (*update)(struct input_stream *is);

	struct tag *(*tag)(struct input_stream *is);

	/**
	 * Returns true if the next read operation will not block:
	 * either data is available, or end-of-stream has been
	 * reached, or an error has occurred.
	 *
	 * If this method is unimplemented, then it is assumed that
	 * reading will never block.
	 */
	bool (*available)(struct input_stream *is);

	size_t (*read)(struct input_stream *is, void *ptr, size_t size,
		       GError **error_r);
	bool (*eof)(struct input_stream *is);
	bool (*seek)(struct input_stream *is, goffset offset, int whence,
		     GError **error_r);
};

#endif
