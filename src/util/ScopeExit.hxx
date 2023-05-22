// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef SCOPE_EXIT_HXX
#define SCOPE_EXIT_HXX

#include <utility>

/**
 * Internal class.  Do not use directly.
 */
template<typename F>
class ScopeExitGuard {
	[[no_unique_address]]
	F function;

	bool enabled = true;

public:
	explicit ScopeExitGuard(F &&f) noexcept
		:function(std::forward<F>(f)) {}

	ScopeExitGuard(ScopeExitGuard &&src) noexcept
		:function(std::move(src.function)),
		 enabled(std::exchange(src.enabled, false)) {}

	/* destructors are "noexcept" by default; this explicit
	   "noexcept" declaration allows the destructor to throw if
	   the function can throw; without this, a throwing function
	   would std::terminate() */
	~ScopeExitGuard() noexcept(noexcept(std::declval<F>()())) {
		if (enabled)
			function();
	}

	ScopeExitGuard(const ScopeExitGuard &) = delete;
	ScopeExitGuard &operator=(const ScopeExitGuard &) = delete;
};

/**
 * Internal class.  Do not use directly.
 */
struct ScopeExitTag {
	/* this operator is a trick so we don't need to close
	   parantheses at the end of the expression AtScopeExit()
	   call */
	template<typename F>
	ScopeExitGuard<F> operator+(F &&f) noexcept {
		return ScopeExitGuard<F>(std::forward<F>(f));
	}
};

#define ScopeExitCat(a, b) a ## b
#define ScopeExitName(line) ScopeExitCat(at_scope_exit_, line)

/**
 * Call the block after this macro at the end of the current scope.
 * Parameters are lambda captures.
 *
 * This is exception-safe, however the given code block must not throw
 * exceptions.
 *
 * This attempts to be a better boost/scope_exit.hpp, without all of
 * Boost's compile-time and runtime bloat.
 */
#define AtScopeExit(...) auto ScopeExitName(__LINE__) = ScopeExitTag() + [__VA_ARGS__]()

#endif
