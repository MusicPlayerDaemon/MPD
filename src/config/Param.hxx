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

#ifndef MPD_CONFIG_PARAM_HXX
#define MPD_CONFIG_PARAM_HXX

#include "util/Compiler.h"

#include <string>

class AllocatedPath;

struct ConfigParam {
	std::string value;

	int line;

	explicit ConfigParam(int _line=-1)
		:line(_line) {}

	template<typename V>
	[[gnu::nonnull]]
	explicit ConfigParam(V &&_value, int _line=-1) noexcept
		:value(std::forward<V>(_value)), line(_line) {}

	ConfigParam(ConfigParam &&) = default;
	ConfigParam &operator=(ConfigParam &&) = default;

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

#endif
