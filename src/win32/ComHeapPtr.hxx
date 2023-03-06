// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
