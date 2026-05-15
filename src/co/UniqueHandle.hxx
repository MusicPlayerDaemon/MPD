// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <coroutine>
#include <utility>

namespace Co {

/**
 * Manage a std::coroutine_handle<> which is destroyed by the
 * destructor.
 */
template<typename Promise=void>
class UniqueHandle {
	std::coroutine_handle<Promise> value;

public:
	UniqueHandle() = default;

	explicit constexpr UniqueHandle(std::coroutine_handle<Promise> h) noexcept
		:value(h) {}

	UniqueHandle(UniqueHandle<Promise> &&src) noexcept
		:value(std::exchange(src.value, nullptr))
	{
	}

	/* this overload allows casting a specialized handle to a
	   std::coroutine_handle<void> */
	template<typename P>
	requires(std::is_void_v<Promise> && !std::is_void_v<P>)
	UniqueHandle(UniqueHandle<P> &&src) noexcept
		:value(src.release())
	{
	}

	~UniqueHandle() noexcept {
		if (value)
			value.destroy();
	}

	auto &operator=(UniqueHandle<Promise> &&src) noexcept {
		using std::swap;
		swap(value, src.value);
		return *this;
	}

	operator bool() const noexcept {
		return (bool)value;
	}

	const auto &get() const noexcept {
		return value;
	}

	const auto *operator->() const noexcept {
		return &value;
	}

#ifdef __clang__
	/* the non-const overload is only needed for clang, because in
	   libc++11, some methods are not "const" */
	auto *operator->() noexcept {
		return &value;
	}
#endif

	[[nodiscard]]
	auto release() noexcept {
		return std::exchange(value, nullptr);
	}
};

} // namespace Co
