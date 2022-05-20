/*
 * Copyright 2013-2022 Max Kellermann <max.kellermann@gmail.com>
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

#pragma once

#include "SpanCast.hxx"

#include <cstddef>
#include <span>
#include <utility>

#ifdef __linux__

/**
 * Allocate a huge amount of memory.  This will be done in a way that
 * allows giving the memory back to the kernel as soon as we don't
 * need it anymore.  On the downside, this call is expensive.
 *
 * Throws std::bad_alloc on error
 *
 * @returns the allocated buffer with a size which may be rounded up
 * (to the next page size), so callers can take advantage of this
 * allocation overhead
 */
std::span<std::byte>
HugeAllocate(size_t size);

/**
 * @param p an allocation returned by HugeAllocate()
 * @param size the allocation's size as passed to HugeAllocate()
 */
void
HugeFree(void *p, size_t size) noexcept;

/**
 * Set a name for the specified virtual memory area.
 *
 * This feature requires Linux 5.17.
 */
void
HugeSetName(void *p, size_t size, const char *name) noexcept;

/**
 * Control whether this allocation is copied to newly forked child
 * processes.  Disabling that makes forking a little bit cheaper.
 */
void
HugeForkCow(void *p, size_t size, bool enable) noexcept;

/**
 * Discard any data stored in the allocation and give the memory back
 * to the kernel.  After returning, the allocation still exists and
 * can be reused at any time, but its contents are undefined.
 *
 * @param p an allocation returned by HugeAllocate()
 * @param size the allocation's size as passed to HugeAllocate()
 */
void
HugeDiscard(void *p, size_t size) noexcept;

#elif defined(_WIN32)
#include <memoryapi.h>

std::span<std::byte>
HugeAllocate(size_t size);

static inline void
HugeFree(void *p, size_t) noexcept
{
	VirtualFree(p, 0, MEM_RELEASE);
}

static inline void
HugeSetName(void *, size_t, const char *) noexcept
{
}

static inline void
HugeForkCow(void *, size_t, bool) noexcept
{
}

static inline void
HugeDiscard(void *p, size_t size) noexcept
{
	VirtualAlloc(p, size, MEM_RESET, PAGE_NOACCESS);
}

#else

/* not Linux: fall back to standard C calls */

#include <cstdint>

static inline std::span<std::byte>
HugeAllocate(size_t size)
{
	return {new std::byte[size], size};
}

static inline void
HugeFree(void *_p, size_t) noexcept
{
	auto *p = (std::byte *)_p;
	delete[] p;
}

static inline void
HugeSetName(void *, size_t, const char *) noexcept
{
}

static inline void
HugeForkCow(void *, size_t, bool) noexcept
{
}

static inline void
HugeDiscard(void *, size_t) noexcept
{
}

#endif

/**
 * Automatic memory management for a dynamic array in "huge" memory.
 */
template<typename T>
class HugeArray {
	using Buffer = std::span<T>;
	Buffer buffer{nullptr};

public:
	typedef typename Buffer::size_type size_type;
	typedef typename Buffer::value_type value_type;
	typedef typename Buffer::reference reference;
	typedef typename Buffer::const_reference const_reference;
	typedef typename Buffer::iterator iterator;

	constexpr HugeArray() = default;

	explicit HugeArray(size_type _size)
		:buffer(FromBytesFloor<value_type>(HugeAllocate(sizeof(value_type) * _size))) {}

	constexpr HugeArray(HugeArray &&other) noexcept
		:buffer(std::exchange(other.buffer, nullptr)) {}

	~HugeArray() noexcept {
		if (!buffer.empty()) {
			auto v = std::as_writable_bytes(buffer);
			HugeFree(v.data(), v.size());
		}
	}

	HugeArray &operator=(HugeArray &&other) noexcept {
		using std::swap;
		swap(buffer, other.buffer);
		return *this;
	}

	void SetName(const char *name) noexcept {
		const auto v = std::as_writable_bytes(buffer);
		HugeSetName(v.data(), v.size(), name);
	}

	void ForkCow(bool enable) noexcept {
		const auto v = std::as_writable_bytes(buffer);
		HugeForkCow(v.data(), v.size(), enable);
	}

	void Discard() noexcept {
		const auto v = std::as_writable_bytes(buffer);
		HugeDiscard(v.data(), v.size());
	}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return buffer == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept {
		return buffer != nullptr;
	}

	/**
	 * Returns the number of allocated elements.
	 */
	constexpr size_type size() const noexcept {
		return buffer.size();
	}

	reference front() noexcept {
		return buffer.front();
	}

	const_reference front() const noexcept {
		return buffer.front();
	}

	reference back() noexcept {
		return buffer.back();
	}

	const_reference back() const noexcept {
		return buffer.back();
	}

	/**
	 * Returns one element.  No bounds checking.
	 */
	reference operator[](size_type i) noexcept {
		return buffer[i];
	}

	/**
	 * Returns one constant element.  No bounds checking.
	 */
	const_reference operator[](size_type i) const noexcept {
		return buffer[i];
	}

	iterator begin() noexcept {
		return buffer.begin();
	}

	constexpr auto begin() const noexcept {
		return buffer.begin();
	}

	iterator end() noexcept {
		return buffer.end();
	}

	constexpr auto end() const noexcept {
		return buffer.end();
	}
};
