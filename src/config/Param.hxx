// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <concepts>
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
	template<std::regular_invocable<const char *> F>
	auto With(F &&f) const {
		try {
			return f(value.c_str());
		} catch (...) {
			ThrowWithNested();
		}
	}
};
