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

#ifndef MPD_INPUT_PLUGIN_HXX
#define MPD_INPUT_PLUGIN_HXX

#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

#include <stddef.h>
#include <stdint.h>

#ifdef WIN32
#include <windows.h>
/* damn you, windows.h! */
#ifdef ERROR
#undef ERROR
#endif
#endif

struct config_param;
struct InputStream;
class Error;
struct Tag;

struct InputPlugin {
	enum class InitResult {
		/**
		 * A fatal error has occurred (e.g. misconfiguration).
		 * The #Error has been set.
		 */
		ERROR,

		/**
		 * The plugin was initialized successfully and is
		 * ready to be used.
		 */
		SUCCESS,

		/**
		 * The plugin is not available and shall be disabled.
		 * The #Error may be set describing the situation (to
		 * be logged).
		 */
		UNAVAILABLE,
	};

	typedef int64_t offset_type;

	const char *name;

	/**
	 * Global initialization.  This method is called when MPD starts.
	 *
	 * @return true on success, false if the plugin should be
	 * disabled
	 */
	InitResult (*init)(const config_param &param, Error &error);

	/**
	 * Global deinitialization.  Called once before MPD shuts
	 * down (only if init() has returned true).
	 */
	void (*finish)(void);

	InputStream *(*open)(const char *uri,
			     Mutex &mutex, Cond &cond,
			     Error &error);
	void (*close)(InputStream *is);

	/**
	 * Check for errors that may have occurred in the I/O thread.
	 * May be unimplemented for synchronous plugins.
	 *
	 * @return false on error
	 */
	bool (*check)(InputStream *is, Error &error);

	/**
	 * Update the public attributes.  Call before access.  Can be
	 * nullptr if the plugin always keeps its attributes up to date.
	 */
	void (*update)(InputStream *is);

	Tag *(*tag)(InputStream *is);

	/**
	 * Returns true if the next read operation will not block:
	 * either data is available, or end-of-stream has been
	 * reached, or an error has occurred.
	 *
	 * If this method is unimplemented, then it is assumed that
	 * reading will never block.
	 */
	bool (*available)(InputStream *is);

	size_t (*read)(InputStream *is, void *ptr, size_t size,
		       Error &error);
	bool (*eof)(InputStream *is);
	bool (*seek)(InputStream *is, offset_type offset, int whence,
		     Error &error);
};

#endif
