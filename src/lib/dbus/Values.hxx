/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#ifndef ODBUS_VALUES_HXX
#define ODBUS_VALUES_HXX

#include "Types.hxx"
#include "util/ConstBuffer.hxx"

#include <dbus/dbus.h>

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
	ConstBuffer<T> value;

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

#endif
