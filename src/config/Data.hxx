/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "Param.hxx"
#include "Block.hxx"

#include <array>
#include <chrono>
#include <forward_list>

class AllocatedPath;

struct ConfigData {
	std::array<std::forward_list<ConfigParam>, std::size_t(ConfigOption::MAX)> params;
	std::array<std::forward_list<ConfigBlock>, std::size_t(ConfigBlockOption::MAX)> blocks;

	void Clear();

	auto &GetParamList(ConfigOption option) noexcept {
		return params[size_t(option)];
	}

	const auto &GetParamList(ConfigOption option) const noexcept {
		return params[size_t(option)];
	}

	void AddParam(ConfigOption option, ConfigParam &&param) noexcept;

	[[gnu::pure]]
	const ConfigParam *GetParam(ConfigOption option) const noexcept {
		const auto &list = GetParamList(option);
		return list.empty() ? nullptr : &list.front();
	}

	template<typename F>
	auto With(ConfigOption option, F &&f) const {
		const auto *param = GetParam(option);
		return param != nullptr
			? param->With(std::forward<F>(f))
			: f(nullptr);
	}

	[[gnu::pure]]
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
		    std::chrono::steady_clock::duration default_value) const;

	unsigned GetPositive(ConfigOption option,
			     unsigned default_value) const;

	std::chrono::steady_clock::duration
	GetPositive(ConfigOption option,
		    std::chrono::steady_clock::duration default_value) const;

	bool GetBool(ConfigOption option, bool default_value) const;

	auto &GetBlockList(ConfigBlockOption option) noexcept {
		return blocks[size_t(option)];
	}

	const auto &GetBlockList(ConfigBlockOption option) const noexcept {
		return blocks[size_t(option)];
	}

	ConfigBlock &AddBlock(ConfigBlockOption option,
			      ConfigBlock &&block) noexcept;

	[[gnu::pure]]
	const ConfigBlock *GetBlock(ConfigBlockOption option) const noexcept {
		const auto &list = GetBlockList(option);
		return list.empty() ? nullptr : &list.front();
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
	[[gnu::pure]]
	const ConfigBlock *FindBlock(ConfigBlockOption option,
				     const char *key, const char *value) const;

	ConfigBlock &MakeBlock(ConfigBlockOption option,
				     const char *key, const char *value);
};

#endif
