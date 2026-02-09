// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <span>

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
