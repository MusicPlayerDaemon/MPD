/*
 * Copyright (C) 2015 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SCOPE_EXIT_HXX
#define SCOPE_EXIT_HXX

#include <utility>

/**
 * Internal class.  Do not use directly.
 */
template<typename F>
class ScopeExitGuard : F {
	bool enabled = true;

public:
	explicit ScopeExitGuard(F &&f):F(std::forward<F>(f)) {}

	ScopeExitGuard(ScopeExitGuard &&src)
		:F(std::move(src)), enabled(src.enabled) {
		src.enabled = false;
	}

	~ScopeExitGuard() {
		if (enabled)
			F::operator()();
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
	ScopeExitGuard<F> operator+(F &&f) {
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
