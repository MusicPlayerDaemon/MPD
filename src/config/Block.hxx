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

#ifndef MPD_CONFIG_BLOCK_HXX
#define MPD_CONFIG_BLOCK_HXX

#include "check.h"
#include "Param.hxx"
#include "Compiler.h"

#include <string>
#include <vector>

class AllocatedPath;

struct BlockParam {
	std::string name;
	std::string value;
	int line;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	mutable bool used;

	gcc_nonnull_all
	BlockParam(const char *_name, const char *_value, int _line=-1)
		:name(_name), value(_value), line(_line), used(false) {}

	int GetIntValue() const;

	unsigned GetUnsignedValue() const;
	unsigned GetPositiveValue() const;

	bool GetBoolValue() const;
};

struct ConfigBlock {
	/**
	 * The next #ConfigBlock with the same name.  The destructor
	 * deletes the whole chain.
	 */
	ConfigBlock *next;

	int line;

	std::vector<BlockParam> block_params;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used;

	explicit ConfigBlock(int _line=-1)
		:next(nullptr), line(_line), used(false) {}

	ConfigBlock(const ConfigBlock &) = delete;

	~ConfigBlock();

	ConfigBlock &operator=(const ConfigBlock &) = delete;

	/**
	 * Determine if this is a "null" instance, i.e. an empty
	 * object that was synthesized and not loaded from a
	 * configuration file.
	 */
	bool IsNull() const noexcept {
		return line < 0;
	}

	gcc_pure
	bool IsEmpty() const noexcept {
		return block_params.empty();
	}

	gcc_nonnull_all
	void AddBlockParam(const char *_name, const char *_value,
			   int _line=-1) {
		block_params.emplace_back(_name, _value, _line);
	}

	gcc_nonnull_all gcc_pure
	const BlockParam *GetBlockParam(const char *_name) const noexcept;

	gcc_pure
	const char *GetBlockValue(const char *name,
				  const char *default_value=nullptr) const noexcept;

	/**
	 * Same as config_get_path(), but looks up the setting in the
	 * specified block.
	 *
	 * Throws #std::runtime_error on error.
	 */
	AllocatedPath GetPath(const char *name,
			      const char *default_value=nullptr) const;

	int GetBlockValue(const char *name, int default_value) const;

	unsigned GetBlockValue(const char *name, unsigned default_value) const;

	unsigned GetPositiveValue(const char *name, unsigned default_value) const;

	bool GetBlockValue(const char *name, bool default_value) const;
};

#endif
