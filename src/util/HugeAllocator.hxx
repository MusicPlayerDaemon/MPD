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

#ifndef HUGE_ALLOCATOR_HXX
#define HUGE_ALLOCATOR_HXX

#include "Compiler.h"

#include <stddef.h>

#ifdef __linux__

/**
 * Allocate a huge amount of memory.  This will be done in a way that
 * allows giving the memory back to the kernel as soon as we don't
 * need it anymore.  On the downside, this call is expensive.
 */
gcc_malloc
void *
HugeAllocate(size_t size);

/**
 * @param p an allocation returned by HugeAllocate()
 * @param size the allocation's size as passed to HugeAllocate()
 */
void
HugeFree(void *p, size_t size);

/**
 * Discard any data stored in the allocation and give the memory back
 * to the kernel.  After returning, the allocation still exists and
 * can be reused at any time, but its contents are undefined.
 *
 * @param p an allocation returned by HugeAllocate()
 * @param size the allocation's size as passed to HugeAllocate()
 */
void
HugeDiscard(void *p, size_t size);

#elif defined(WIN32)
#include <windows.h>

gcc_malloc
static inline void *
HugeAllocate(size_t size)
{
	// TODO: use MEM_LARGE_PAGES
	return VirtualAlloc(nullptr, size,
			    MEM_COMMIT|MEM_RESERVE,
			    PAGE_READWRITE);
}

static inline void
HugeFree(void *p, gcc_unused size_t size)
{
	VirtualFree(p, 0, MEM_RELEASE);
}

static inline void
HugeDiscard(void *p, size_t size)
{
	VirtualAlloc(p, size, MEM_RESET, PAGE_NOACCESS);
}

#else

/* not Linux: fall back to standard C calls */

#include <stdlib.h>

gcc_malloc
static inline void *
HugeAllocate(size_t size)
{
	return malloc(size);
}

static inline void
HugeFree(void *p, size_t)
{
	free(p);
}

static inline void
HugeDiscard(void *, size_t)
{
}

#endif

#endif
