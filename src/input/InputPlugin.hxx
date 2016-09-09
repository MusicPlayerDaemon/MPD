/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

struct ConfigBlock;
class InputStream;
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
	};

	const char *name;

	/**
	 * Global initialization.  This method is called when MPD starts.
	 *
	 * Throws #PluginUnavailable if the plugin is not available
	 * and shall be disabled.
	 */
	InitResult (*init)(const ConfigBlock &block, Error &error);

	/**
	 * Global deinitialization.  Called once before MPD shuts
	 * down (only if init() has returned true).
	 */
	void (*finish)();

	InputStream *(*open)(const char *uri,
			     Mutex &mutex, Cond &cond,
			     Error &error);
};

#endif
