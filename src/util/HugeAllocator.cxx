// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "HugeAllocator.hxx"
#include "system/VmaName.hxx"
#include "util/RoundPowerOfTwo.hxx"

#include <new>

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#else
#include <stdlib.h>
#endif

#ifdef __linux__

/**
 * Round up the parameter, make it page-aligned.
 */
[[gnu::const]]
static size_t
AlignToPageSize(size_t size) noexcept
{
	static const long page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0)
		return size;

	return RoundUpToPowerOfTwo(size, static_cast<std::size_t>(page_size));
}

std::span<std::byte>
HugeAllocate(size_t size)
{
	size = AlignToPageSize(size);

	constexpr int flags = MAP_ANONYMOUS|MAP_PRIVATE|MAP_NORESERVE;
	void *p = mmap(nullptr, size,
		       PROT_READ|PROT_WRITE, flags,
		       -1, 0);
	if (p == (void *)-1)
		throw std::bad_alloc();

#ifdef MADV_HUGEPAGE
	/* allow the Linux kernel to use "Huge Pages", which reduces page
	   table overhead for this big chunk of data */
	madvise(p, size, MADV_HUGEPAGE);
#endif

	return {(std::byte *)p, size};
}

void
HugeFree(void *p, size_t size) noexcept
{
	munmap(p, AlignToPageSize(size));
}

void
HugeSetName(void *p, size_t size, const char *name) noexcept
{
	SetVmaName(p, size, name);
}

void
HugeForkCow(void *p, size_t size, bool enable) noexcept
{
#ifdef MADV_DONTFORK
	madvise(p, AlignToPageSize(size),
		enable ? MADV_DOFORK : MADV_DONTFORK);
#endif
}

void
HugeDiscard(void *p, size_t size) noexcept
{
#ifdef MADV_DONTNEED
	madvise(p, AlignToPageSize(size), MADV_DONTNEED);
#endif
}

#elif defined(_WIN32)

std::span<std::byte>
HugeAllocate(size_t size)
{
	// TODO: use MEM_LARGE_PAGES
	void *p = VirtualAlloc(nullptr, size,
			       MEM_COMMIT|MEM_RESERVE,
			       PAGE_READWRITE);
	if (p == nullptr)
		throw std::bad_alloc();

	// TODO: round size up to the page size
	return {(std::byte *)p, size};
}

#endif
