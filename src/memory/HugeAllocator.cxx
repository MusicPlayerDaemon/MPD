// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "HugeAllocator.hxx"

#ifdef __linux__

#include "system/PageAllocator.hxx"
#include "system/PageSize.hxx"
#include "system/VmaName.hxx"

static std::span<std::byte>
AlignToPageSize(std::span<std::byte> p) noexcept
{
	return {p.data(), AlignToPageSize(p.size())};
}

std::span<std::byte>
HugeAllocate(size_t size)
{
	size = AlignToPageSize(size);

	const std::span<std::byte> p{AllocatePages(size), size};
	EnableHugePages(p);
	return p;
}

void
HugeFree(std::span<std::byte> p) noexcept
{
	FreePages(AlignToPageSize(p));
}

void
HugeSetName(std::span<std::byte> p, const char *name) noexcept
{
	SetVmaName(p.data(), p.size(), name);
}

void
HugeForkCow(std::span<std::byte> p, bool enable) noexcept
{
	EnablePageFork(AlignToPageSize(p), enable);
}

void
HugePopulate(std::span<std::byte> p) noexcept
{
	PagesPopulateWrite(p);
	CollapseHugePages(p);
}

void
HugeDiscard(std::span<std::byte> p) noexcept
{
	DiscardPages(AlignToPageSize(p));
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
