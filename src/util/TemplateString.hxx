/*
 * Copyright (C) 2015-2016 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef TEMPLATE_STRING_HXX
#define TEMPLATE_STRING_HXX

#include <stddef.h>

namespace TemplateString {
	/**
	 * Construct a null-terminated string from a list of chars.
	 */
	template<char... _value>
	struct Construct {
		static constexpr char value[] = {_value..., 0};
		static constexpr size_t size = sizeof...(_value);
	};

	template<char... _value>
	constexpr char Construct<_value...>::value[];

	/**
	 * An empty string.
	 */
	struct Empty : Construct<> {};

	/**
	 * A string consisting of a single character.
	 */
	template<char ch>
	struct CharAsString : Construct<ch> {};

	/**
	 * Invoke #F, pass all characters in #src from #i to #length
	 * as variadic arguments.
	 */
	template<template<char...> class F,
		 const char *src, size_t length, size_t i,
		 char... _value>
	struct VariadicChars : VariadicChars<F, src, length - 1, i + 1, _value..., src[i]> {
		static_assert(length > 0, "Wrong length");
	};

	template<template<char...> class F,
		 const char *src, size_t length,
		 char... _value>
	struct VariadicChars<F, src, 0, length, _value...> : F<_value...> {};

	/**
	 * Like #VariadicChars, but pass an additional argument to #F.
	 */
	template<template<typename Arg, char...> class F, typename Arg,
		 const char *src, size_t length, size_t i,
		 char... _value>
	struct VariadicChars1 : VariadicChars1<F, Arg,
					       src, length - 1, i + 1, _value..., src[i]> {
		static_assert(length > 0, "Wrong length");
	};

	template<template<typename Arg, char...> class F, typename Arg,
		 const char *src, size_t length,
		 char... _value>
	struct VariadicChars1<F, Arg, src, 0, length, _value...> : F<Arg, _value...> {};

	template<const char *src, size_t length, char... value>
	struct _BuildString : VariadicChars<Construct, src, length, 0,
					    value...> {};

	template<char ch, typename S>
	struct InsertBefore : _BuildString<S::value, S::size, ch> {};

	/**
	 * Concatenate several strings.
	 */
	template<typename... Args>
	struct Concat;

	template<typename First, typename Second, typename... Args>
	struct _Concat : Concat<Concat<First, Second>, Args...> {};

	template<typename... Args>
	struct Concat : _Concat<Args...> {};

	template<typename Second, char... _value>
	struct _Concat2 : _BuildString<Second::value, Second::size,
				      _value...> {};

	template<typename First, typename Second>
	struct Concat<First, Second>
		:VariadicChars1<_Concat2, Second,
				First::value, First::size, 0> {};

	template<typename First>
	struct Concat<First> : First {};

	template<>
	struct Concat<> : Empty {};
};

#endif
