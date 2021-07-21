/*
 * Copyright 2020-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_WIN32_COMHEAPPTR_HXX
#define MPD_WIN32_COMHEAPPTR_HXX

#include <cstddef>
#include <objbase.h>
#include <utility>

#include <combaseapi.h>

// RAII for CoTaskMemAlloc and CoTaskMemFree
// https://docs.microsoft.com/zh-tw/windows/win32/api/combaseapi/nf-combaseapi-cotaskmemalloc
// https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cotaskmemfree
template <typename T>
class ComHeapPtr {
public:
	using pointer = T *;
	using element_type = T;

	constexpr ComHeapPtr() noexcept : ptr(nullptr) {}
	constexpr ComHeapPtr(std::nullptr_t) noexcept : ptr(nullptr) {}
	explicit constexpr ComHeapPtr(pointer p) noexcept : ptr(p) {}

	ComHeapPtr(const ComHeapPtr &u) = delete;
	constexpr ComHeapPtr(ComHeapPtr &&u) noexcept : ptr(std::exchange(u.ptr, nullptr)) {}

	ComHeapPtr &operator=(const ComHeapPtr &r) = delete;
	constexpr ComHeapPtr &operator=(ComHeapPtr &&r) noexcept {
		std::swap(ptr, r.ptr);
		return *this;
	}
	ComHeapPtr &operator=(std::nullptr_t) noexcept {
		reset();
		return *this;
	}

	~ComHeapPtr() noexcept { reset(); }

	pointer release() noexcept { return std::exchange(ptr, nullptr); }
	void reset() noexcept {
		if (ptr) {
			CoTaskMemFree(release());
		}
	}
	void swap(ComHeapPtr &other) noexcept { std::swap(ptr, other.ptr); }

	pointer get() const noexcept { return ptr; }
	explicit operator bool() const noexcept { return ptr; }

	auto operator*() const { return *ptr; }
	pointer operator->() const noexcept { return ptr; }

	T **Address() noexcept {
		reset();
		return &ptr;
	}
	template <typename U = void>
	U **AddressCast() noexcept {
		reset();
		return reinterpret_cast<U **>(&ptr);
	}

	template <typename U = void>
	U *Cast() noexcept {
		return reinterpret_cast<U *>(ptr);
	}

private:
	pointer ptr;
};

namespace std {
template <typename T>
void swap(ComHeapPtr<T> &lhs, ComHeapPtr<T> &rhs) noexcept {
	lhs.swap(rhs);
}
}

#endif
