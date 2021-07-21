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

#ifndef MPD_WIN32_COMPTR_HXX
#define MPD_WIN32_COMPTR_HXX

#include "win32/HResult.hxx"

#include <cstddef>
#include <objbase.h>
#include <utility>

#include <combaseapi.h>

// RAII for Object in Microsoft Component Object Model(COM)
// https://docs.microsoft.com/zh-tw/windows/win32/api/_com/
template <typename T>
class ComPtr {
public:
	using pointer = T *;
	using reference = T &;
	using element_type = T;

	constexpr ComPtr() noexcept : ptr(nullptr) {}
	constexpr ComPtr(std::nullptr_t) noexcept : ptr(nullptr) {}
	explicit constexpr ComPtr(pointer p) noexcept : ptr(p) {}

	ComPtr(const ComPtr &u) noexcept : ptr(u.ptr) {
		if (ptr) {
			ptr->AddRef();
		}
	}
	constexpr ComPtr(ComPtr &&u) noexcept : ptr(std::exchange(u.ptr, nullptr)) {}

	ComPtr &operator=(const ComPtr &r) noexcept {
		reset();
		ptr = r.ptr;
		if (ptr) {
			ptr->AddRef();
		}
		return *this;
	}
	constexpr ComPtr &operator=(ComPtr &&r) noexcept {
		std::swap(ptr, r.ptr);
		return *this;
	}
	ComPtr &operator=(std::nullptr_t) noexcept {
		reset();
		return *this;
	}

	~ComPtr() noexcept { reset(); }

	pointer release() noexcept { return std::exchange(ptr, nullptr); }
	void reset() noexcept {
		if (ptr) {
			release()->Release();
		}
	}
	void swap(ComPtr &other) noexcept { std::swap(ptr, other.ptr); }

	pointer get() const noexcept { return ptr; }
	explicit operator bool() const noexcept { return ptr; }

	reference operator*() const noexcept { return *ptr; }
	pointer operator->() const noexcept { return ptr; }

	void CoCreateInstance(REFCLSID class_id, LPUNKNOWN unknown_outer = nullptr,
			      DWORD class_context = CLSCTX_ALL) {
		HRESULT result =
			::CoCreateInstance(class_id, unknown_outer, class_context,
					   __uuidof(T), reinterpret_cast<void **>(&ptr));
		if (FAILED(result)) {
			throw MakeHResultError(result, "Unable to create instance");
		}
	}

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
void swap(ComPtr<T> &lhs, ComPtr<T> &rhs) noexcept {
	lhs.swap(rhs);
}
} // namespace std

#endif
