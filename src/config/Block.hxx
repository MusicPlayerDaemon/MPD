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

#ifndef MPD_CONFIG_BLOCK_HXX
#define MPD_CONFIG_BLOCK_HXX

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
	mutable bool used = false;

	template<typename N, typename V>
	[[gnu::nonnull]]
	BlockParam(N &&_name, V &&_value, int _line=-1) noexcept
		:name(std::forward<N>(_name)), value(std::forward<V>(_value)),
		 line(_line) {}

	int GetIntValue() const;

	unsigned GetUnsignedValue() const;
	unsigned GetPositiveValue() const;

	bool GetBoolValue() const;

	/**
	 * Call this method in a "catch" block to throw a nested
	 * exception showing the location of this setting in the
	 * configuration file.
	 */
	[[noreturn]]
	void ThrowWithNested() const;

	/**
	 * Invoke a function with the configured value; if the
	 * function throws, call ThrowWithNested().
	 */
	template<typename F>
	auto With(F &&f) const {
		try {
			return f(value.c_str());
		} catch (...) {
			ThrowWithNested();
		}
	}
};

struct ConfigBlock {
	int line;

	std::vector<BlockParam> block_params;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	mutable bool used = false;

	explicit ConfigBlock(int _line=-1)
		:line(_line) {}

	ConfigBlock(ConfigBlock &&) = default;
	ConfigBlock &operator=(ConfigBlock &&) = default;

	/**
	 * Determine if this is a "null" instance, i.e. an empty
	 * object that was synthesized and not loaded from a
	 * configuration file.
	 */
	bool IsNull() const noexcept {
		return line < 0;
	}

	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return block_params.empty();
	}

	void SetUsed() const noexcept {
		used = true;
	}

	template<typename N, typename V>
	[[gnu::nonnull]]
	void AddBlockParam(N &&_name, V &&_value, int _line=-1) noexcept {
		block_params.emplace_back(std::forward<N>(_name),
					  std::forward<V>(_value),
					  _line);
	}

	[[gnu::nonnull]] [[gnu::pure]]
	const BlockParam *GetBlockParam(const char *_name) const noexcept;

	[[gnu::pure]]
	const char *GetBlockValue(const char *name,
				  const char *default_value=nullptr) const noexcept;

	/**
	 * Same as ConfigData::GetPath(), but looks up the setting in the
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
