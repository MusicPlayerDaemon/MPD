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
 */
void
HugeFree(std::span<std::byte> p) noexcept;

/**
 * Set a name for the specified virtual memory area.
 *
 * This feature requires Linux 5.17.
 */
void
HugeSetName(std::span<std::byte> p, const char *name) noexcept;

/**
 * Control whether this allocation is copied to newly forked child
 * processes.  Disabling that makes forking a little bit cheaper.
 */
void
HugeForkCow(std::span<std::byte> p, bool enable) noexcept;

/**
 * Populate (prefault) page tables writable, faulting in all pages in
 * the range just as if manually writing to each each page.
 */
void
HugePopulate(std::span<std::byte> p) noexcept;

/**
 * Discard any data stored in the allocation and give the memory back
 * to the kernel.  After returning, the allocation still exists and
 * can be reused at any time, but its contents are undefined.
 *
 * @param p an allocation returned by HugeAllocate()
 */
void
HugeDiscard(std::span<std::byte> p) noexcept;

#elif defined(_WIN32)
#include <memoryapi.h>

std::span<std::byte>
HugeAllocate(size_t size);

static inline void
HugeFree(std::span<std::byte> p) noexcept
{
	VirtualFree(p.data(), 0, MEM_RELEASE);
}

static inline void
HugeSetName(std::span<std::byte>, const char *) noexcept
{
}

static inline void
HugeForkCow(std::span<std::byte>, bool) noexcept
{
}

static inline void
HugePopulate(std::span<std::byte> p) noexcept
{
	VirtualAlloc(p.data(), p.size(), MEM_COMMIT|MEM_RESERVE, PAGE_NOACCESS);
}

static inline void
HugeDiscard(std::span<std::byte> p) noexcept
{
	VirtualAlloc(p.data(), p.size(), MEM_RESET, PAGE_NOACCESS);
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
HugeFree(std::span<std::byte> p) noexcept
{
	delete[] p.data();
}

static inline void
HugeSetName(std::span<std::byte>, const char *) noexcept
{
}

static inline void
HugeForkCow(std::span<std::byte>, bool) noexcept
{
}

static inline void
HugePopulate(std::span<std::byte>) noexcept
{
}

static inline void
HugeDiscard(std::span<std::byte>) noexcept
{
}

#endif
