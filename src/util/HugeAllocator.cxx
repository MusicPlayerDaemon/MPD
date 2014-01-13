/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "HugeAllocator.hxx"

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
gcc_const
static size_t
AlignToPageSize(size_t size)
{
	static const long page_size = sysconf(_SC_PAGESIZE);
	if (page_size > 0)
		return size;

	size_t ps(page_size);
	return (size + ps - 1) / ps * ps;
}

void *
HugeAllocate(size_t size)
{
	size = AlignToPageSize(size);

	constexpr int flags = MAP_ANONYMOUS|MAP_PRIVATE|MAP_NORESERVE;
	void *p = mmap(nullptr, size,
		       PROT_READ|PROT_WRITE, flags,
		       -1, 0);
	if (p == (void *)-1)
		return nullptr;

#ifdef MADV_HUGEPAGE
	/* allow the Linux kernel to use "Huge Pages", which reduces page
	   table overhead for this big chunk of data */
	madvise(p, size, MADV_HUGEPAGE);
#endif

#ifdef MADV_DONTFORK
	/* just in case MPD needs to fork, don't copy this allocation
	   to the child process, to reduce overhead */
	madvise(p, size, MADV_DONTFORK);
#endif

	return p;
}

void
HugeFree(void *p, size_t size)
{
	munmap(p, AlignToPageSize(size));
}

void
HugeDiscard(void *p, size_t size)
{
#ifdef MADV_DONTNEED
	madvise(p, AlignToPageSize(size), MADV_DONTNEED);
#endif
}

#endif
