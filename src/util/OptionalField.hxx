// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <utility> // for std::forward()

/**
 * Helps with declaring a field that is present only under a certain
 * (compile-time) condition.  If `enable` is true, this struct
 * contains a field named `value` of the specified type.  To avoid
 * memory overhead when disabled, add the attribute
 * [[no_unique_address]].
 */
template<typename T, bool enable> struct OptionalField;

template<typename T>
struct OptionalField<T, false>
{
	template<typename... Args>
	constexpr OptionalField(Args&&...) {}
};

template<typename T>
struct OptionalField<T, true>
{
	T value;

	template<typename... Args>
	constexpr OptionalField(Args&&... args)
		:value(std::forward<Args>(args)...) {}
};
