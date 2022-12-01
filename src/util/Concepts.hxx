/*
 * Copyright 2022 Max Kellermann <max.kellermann@gmail.com>
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

#pragma once

#include <concepts>

/**
 * Compatibility wrapper for std::invocable which is unavailable in
 * the Android NDK r25b and Apple Xcode.
 */
#if !defined(ANDROID) && !defined(__APPLE__)
template<typename F, typename... Args>
concept Invocable = std::invocable<F, Args...>;
#else
template<typename F, typename... Args>
concept Invocable = requires(F f, Args... args) {
	{ f(args...) };
};
#endif

/**
 * Compatibility wrapper for std::predicate which is unavailable in
 * the Android NDK r25b and Apple Xcode.
 */
#if !defined(ANDROID) && !defined(__APPLE__)
template<typename F, typename... Args>
concept Predicate = std::predicate<F, Args...>;
#else
template<typename F, typename... Args>
concept Predicate = requires(F f, Args... args) {
	{ f(args...) } -> std::same_as<bool>;
};
#endif

template<typename F, typename T>
concept Disposer = Invocable<F, T *>;
