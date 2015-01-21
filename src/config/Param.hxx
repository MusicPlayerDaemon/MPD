/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "Block.hxx"
#include "Compiler.h"

#include <string>
#include <vector>

class AllocatedPath;
class Error;

struct config_param {
	/**
	 * The next config_param with the same name.  The destructor
	 * deletes the whole chain.
	 */
	struct config_param *next;

	std::string value;

	int line;

	std::vector<BlockParam> block_params;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used;

	explicit config_param(int _line=-1)
		:next(nullptr), line(_line), used(false) {}

	gcc_nonnull_all
	config_param(const char *_value, int _line=-1);

	config_param(const config_param &) = delete;

	~config_param();

	config_param &operator=(const config_param &) = delete;

	/**
	 * Determine if this is a "null" instance, i.e. an empty
	 * object that was synthesized and not loaded from a
	 * configuration file.
	 */
	bool IsNull() const {
		return line < 0;
	}

	gcc_nonnull_all
	void AddBlockParam(const char *_name, const char *_value,
			   int _line=-1) {
		block_params.emplace_back(_name, _value, _line);
	}

	gcc_nonnull_all gcc_pure
	const BlockParam *GetBlockParam(const char *_name) const;

	gcc_pure
	const char *GetBlockValue(const char *name,
				  const char *default_value=nullptr) const;

	/**
	 * Same as config_get_path(), but looks up the setting in the
	 * specified block.
	 */
	AllocatedPath GetBlockPath(const char *name, const char *default_value,
				   Error &error) const;

	AllocatedPath GetBlockPath(const char *name, Error &error) const;

	gcc_pure
	int GetBlockValue(const char *name, int default_value) const;

	gcc_pure
	unsigned GetBlockValue(const char *name, unsigned default_value) const;

	gcc_pure
	bool GetBlockValue(const char *name, bool default_value) const;
};

#endif
