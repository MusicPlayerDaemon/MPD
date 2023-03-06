// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
