/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef MPD_CONFIG_DATA_HXX
#define MPD_CONFIG_DATA_HXX

#include "Option.hxx"

#include <array>
#include <chrono>
#include <memory>

struct ConfigParam;
struct ConfigBlock;
class AllocatedPath;

struct ConfigData {
	std::array<ConfigParam *, std::size_t(ConfigOption::MAX)> params{{nullptr}};
	std::array<ConfigBlock *, std::size_t(ConfigBlockOption::MAX)> blocks{{nullptr}};

	void Clear();

	void AddParam(ConfigOption option,
		      std::unique_ptr<ConfigParam> param) noexcept;

	gcc_pure
	const ConfigParam *GetParam(ConfigOption option) const noexcept {
		return params[size_t(option)];
	}

	gcc_pure
	const char *GetString(ConfigOption option,
			      const char *default_value=nullptr) const noexcept;

	/**
	 * Returns an optional configuration variable which contains an
	 * absolute path.  If there is a tilde prefix, it is expanded.
	 * Returns nullptr if the value is not present.
	 *
	 * Throws #std::runtime_error on error.
	 */
	AllocatedPath GetPath(ConfigOption option) const;

	unsigned GetUnsigned(ConfigOption option,
			     unsigned default_value) const;

	std::chrono::steady_clock::duration
	GetUnsigned(ConfigOption option,
		    std::chrono::steady_clock::duration default_value) const {
		// TODO: allow unit suffixes
		auto u = GetUnsigned(option, default_value.count());
		return std::chrono::steady_clock::duration(u);
	}

	unsigned GetPositive(ConfigOption option,
			     unsigned default_value) const;

	std::chrono::steady_clock::duration
	GetPositive(ConfigOption option,
		    std::chrono::steady_clock::duration default_value) const {
		// TODO: allow unit suffixes
		auto u = GetPositive(option, default_value.count());
		return std::chrono::steady_clock::duration(u);
	}

	bool GetBool(ConfigOption option, bool default_value) const;

	void AddBlock(ConfigBlockOption option,
		      std::unique_ptr<ConfigBlock> block) noexcept;

	gcc_pure
	const ConfigBlock *GetBlock(ConfigBlockOption option) const noexcept {
		return blocks[size_t(option)];
	}

	/**
	 * Find a block with a matching attribute.
	 *
	 * Throws if a block doesn't have the specified (mandatory) key.
	 *
	 * @param option the blocks to search
	 * @param key the attribute name
	 * @param value the expected attribute value
	 */
	gcc_pure
	const ConfigBlock *FindBlock(ConfigBlockOption option,
				     const char *key, const char *value) const;
};

#endif
