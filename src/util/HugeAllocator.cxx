/*
 * Copyright (C) 2013 Max Kellermann <max@duempel.org>
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
