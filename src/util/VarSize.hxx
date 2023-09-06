// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

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
requires std::is_standard_layout_v<T>
[[gnu::malloc]] [[gnu::returns_nonnull]]
T *
NewVarSize(size_t declared_tail_size, size_t real_tail_size, Args&&... args)
{
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
[[gnu::nonnull]]
void
DeleteVarSize(T *instance)
{
	/* call the destructor */
	instance->T::~T();

	/* free memory */
	free(instance);
}
