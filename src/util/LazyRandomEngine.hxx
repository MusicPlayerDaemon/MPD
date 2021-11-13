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

#ifndef MPD_LAZY_RANDOM_ENGINE_HXX
#define MPD_LAZY_RANDOM_ENGINE_HXX

#include <cassert>
#include <optional>
#include <random>

/**
 * A random engine that will be created and seeded on demand.
 */
class LazyRandomEngine {
	std::optional<std::mt19937> engine;

public:
	typedef std::mt19937::result_type result_type;

	LazyRandomEngine() : engine(std::nullopt) {}

	LazyRandomEngine(const LazyRandomEngine &other) = delete;
	LazyRandomEngine &operator=(const LazyRandomEngine &other) = delete;

	/**
	 * Create and seed the real engine.  Call this before any
	 * other method.
	 */
	void AutoCreate();

	static constexpr result_type min() { return std::mt19937::min(); }

	static constexpr result_type max() { return std::mt19937::max(); }

	result_type operator()() {
		assert(engine);

		return engine->operator()();
	}
};

#endif
