// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstddef>
#include <span>

#include <sys/mman.h>

/**
 * Allocate pages from the kernel
 *
 * Throws std::bad_alloc on error.
 *
 * @param size the size of the allocation; must be a multiple of
 * #PAGE_SIZE
 */
std::byte *
AllocatePages(std::size_t size);

static inline void
FreePages(std::span<std::byte> p) noexcept
{
	munmap(p.data(), p.size());
}

/**
 * Allow the Linux kernel to use "Huge Pages" for the cache, which
 * reduces page table overhead for this big chunk of data.
 *
 * @param size a multiple of #HUGE_PAGE_SIZE
 */
static inline void
EnableHugePages(std::span<std::byte> p) noexcept
{
#ifdef MADV_HUGEPAGE
	madvise(p.data(), p.size(), MADV_HUGEPAGE);
#else
	(void)p;
#endif
}

/**
 * Attempt to collapse all regular pages into transparent huge pages.
 *
 * @param size a multiple of #HUGE_PAGE_SIZE
 */
static inline void
CollapseHugePages(std::span<std::byte> p) noexcept
{
#ifdef MADV_COLLAPSE
	madvise(p.data(), p.size(), MADV_COLLAPSE);
#else
	(void)p;
#endif
}

/**
 * Controls whether forked processes inherit the specified pages.
 */
static inline void
EnablePageFork(std::span<std::byte> p, bool inherit) noexcept
{
#ifdef __linux__
	madvise(p.data(), p.size(), inherit ? MADV_DOFORK : MADV_DONTFORK);
#else
	(void)p;
	(void)inherit;
#endif
}

/**
 * Controls whether the specified pages will be included in a core
 * dump.
 */
static inline void
EnablePageDump(std::span<std::byte> p, bool dump) noexcept
{
#ifdef __linux__
	madvise(p.data(), p.size(), dump ? MADV_DODUMP : MADV_DONTDUMP);
#else
	(void)p;
	(void)dump;
#endif
}

/**
 * Discard the specified page contents, giving memory back to the
 * kernel.  The mapping is preserved, and new memory will be allocated
 * automatically on the next write access.
 */
static inline void
DiscardPages(std::span<std::byte> p) noexcept
{
#ifdef __linux__
	madvise(p.data(), p.size(), MADV_DONTNEED);
#else
	(void)p;
#endif
}

/**
 * Populate (prefault) page tables writable, faulting in all pages in
 * the range just as if manually writing to each each page.
 */
static inline void
PagesPopulateWrite(std::span<std::byte> p) noexcept
{
#ifdef MADV_POPULATE_WRITE
	madvise(p.data(), p.size(), MADV_POPULATE_WRITE);
#else
	(void)p;
#endif
}
