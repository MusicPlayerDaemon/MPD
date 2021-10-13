/*
 * Copyright (C) 2008-2014 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef MPD_VAR_SIZE_HXX
#define MPD_VAR_SIZE_HXX

#include "Compiler.h"

#include <type_traits>
#include <utility>
#include <new>
#include <cstdlib>

/**
 * Allocate and construct a variable-size object.  That is useful for
 * example when you want to store a variable-length string as the last
 * attribute without the overhead of a second allocation.
 *
 * @tparam T a struct/class with a variable-size last attribute
 * @param declared_tail_size the declared size of the last element in
 * #T
 * @param real_tail_size the real required size of the last element in
 * #T
 */
template<class T, typename... Args>
gcc_malloc gcc_returns_nonnull
T *
NewVarSize(size_t declared_tail_size, size_t real_tail_size, Args&&... args)
{
	static_assert(std::is_standard_layout<T>::value,
		      "Not standard-layout");

	/* determine the total size of this instance */
	size_t size = sizeof(T) - declared_tail_size + real_tail_size;

	/* allocate memory */
	T *instance = (T *)malloc(size);
	if (instance == nullptr)
		throw std::bad_alloc{};

	/* call the constructor */
	new(instance) T(std::forward<Args>(args)...);

	return instance;
}

template<typename T>
gcc_nonnull_all
void
DeleteVarSize(T *instance)
{
	/* call the destructor */
	instance->T::~T();

	/* free memory */
	free(instance);
}

#endif
