// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Types.hxx"

#include <dbus/dbus.h>

#include <span>
#include <tuple>

namespace ODBus {

template<typename T>
struct BasicValue {
	using Traits = TypeTraits<T>;
	const T &value;

	explicit constexpr BasicValue(const T &_value) noexcept
		:value(_value) {}
};

struct String : BasicValue<const char *> {
	explicit constexpr String(const char *const&_value) noexcept
		:BasicValue(_value) {}
};

struct Boolean {
	using Traits = BooleanTypeTraits;
	dbus_bool_t value;

	explicit constexpr Boolean(bool _value) noexcept
		:value(_value) {}
};

using Uint32 = BasicValue<dbus_uint32_t>;
using Uint64 = BasicValue<dbus_uint64_t>;

template<typename T, template<typename U> class WrapTraits>
struct WrapValue {
	using ContainedTraits = typename T::Traits;
	using Traits = WrapTraits<ContainedTraits>;
	const T &value;

	explicit constexpr WrapValue(const T &_value) noexcept
		:value(_value) {}
};

template<typename T>
struct WrapVariant : BasicValue<T> {
	using ContainedTraits = typename T::Traits;
	using Traits = VariantTypeTraits;

	explicit constexpr WrapVariant(const T &_value) noexcept
		:BasicValue<T>(_value) {}
};

template<typename T>
static WrapVariant<T> Variant(const T &_value) noexcept {
	return WrapVariant<T>(_value);
}

template<typename T>
struct WrapFixedArray {
	using ContainedTraits = TypeTraits<T>;
	using Traits = ArrayTypeTraits<ContainedTraits>;
	std::span<const T> value;

	explicit constexpr WrapFixedArray(const T *_data,
					  size_t _size) noexcept
		:value(_data, _size) {}
};

template<typename T>
static WrapFixedArray<T> FixedArray(const T *_data,
				    size_t _size) noexcept {
	return WrapFixedArray<T>(_data, _size);
}

template<typename... T>
struct WrapStruct {
	using Traits = StructTypeTraits<typename T::Traits...>;

	std::tuple<const T&...> values;

	explicit constexpr WrapStruct(const T&... _values) noexcept
		:values(_values...) {}
};

template<typename... T>
static WrapStruct<T...> Struct(const T&... values) noexcept {
	return WrapStruct<T...>(values...);
}

} /* namespace ODBus */
