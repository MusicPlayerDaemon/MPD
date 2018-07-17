/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_CONFIG_PARAM_HXX
#define MPD_CONFIG_PARAM_HXX

#include "check.h"
#include "Compiler.h"

#include <string>

class AllocatedPath;

struct ConfigParam {
	/**
	 * The next ConfigParam with the same name.  The destructor
	 * deletes the whole chain.
	 */
	ConfigParam *next = nullptr;

	std::string value;

	int line;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used = false;

	explicit ConfigParam(int _line=-1)
		:line(_line) {}

	gcc_nonnull_all
	ConfigParam(const char *_value, int _line=-1);

	ConfigParam(const ConfigParam &) = delete;

	~ConfigParam();

	ConfigParam &operator=(const ConfigParam &) = delete;

	/**
	 * Determine if this is a "null" instance, i.e. an empty
	 * object that was synthesized and not loaded from a
	 * configuration file.
	 */
	bool IsNull() const {
		return line < 0;
	}

	/**
	 * Parse the value as a path.  If there is a tilde prefix, it
	 * is expanded.
	 *
	 * Throws #std::runtime_error on error.
	 */
	AllocatedPath GetPath() const;
};

#endif
