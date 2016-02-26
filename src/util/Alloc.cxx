/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "Alloc.hxx"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

gcc_noreturn
static void
oom()
{
	(void)write(STDERR_FILENO, "Out of memory\n", 14);
	_exit(1);
}

void *
xalloc(size_t size)
{
	void *p = malloc(size);
	if (gcc_unlikely(p == nullptr))
		oom();

	return p;
}

void *
xmemdup(const void *s, size_t size)
{
	void *p = xalloc(size);
	memcpy(p, s, size);
	return p;
}

char *
xstrdup(const char *s)
{
	char *p = strdup(s);
	if (gcc_unlikely(p == nullptr))
		oom();

	return p;
}

char *
xstrndup(const char *s, size_t n)
{
#ifdef HAVE_STRNDUP
	char *p = strndup(s, n);
	if (gcc_unlikely(p == nullptr))
		oom();
#else
	char *p = (char *)xalloc(n + 1);
	memcpy(p, s, n);
	p[n] = 0;
#endif

	return p;
}

#if CLANG_OR_GCC_VERSION(4,7)

template<typename... Args>
static inline size_t
FillLengths(size_t *lengths, const char *a, Args&&... args)
{
	return FillLengths(lengths, a) + FillLengths(lengths + 1, args...);
}

template<>
inline size_t
FillLengths(size_t *lengths, const char *a)
{
	return *lengths = strlen(a);
}

template<typename... Args>
static inline void
StringCat(char *p, const size_t *lengths, const char *a, Args&&... args)
{
	StringCat(p, lengths, a);
	StringCat(p + *lengths, lengths + 1, args...);
}

template<>
inline void
StringCat(char *p, const size_t *lengths, const char *a)
{
	memcpy(p, a, *lengths);
}

#endif

template<typename... Args>
gcc_malloc gcc_nonnull_all
static inline char *
t_xstrcatdup(Args&&... args)
{
#if CLANG_OR_GCC_VERSION(4,7)
	constexpr size_t n = sizeof...(args);

	size_t lengths[n];
	const size_t total = FillLengths(lengths, args...);

	char *p = (char *)xalloc(total + 1);
	StringCat(p, lengths, args...);
	p[total] = 0;
	return p;
#else
	/* fallback implementation for gcc 4.6, because that old
	   compiler is too buggy to compile the above template
	   functions */
	const char *const argv[] = { args... };

	size_t total = 0;
	for (auto i : argv)
		total += strlen(i);

	char *p = (char *)xalloc(total + 1), *q = p;
	for (auto i : argv)
		q = stpcpy(q, i);

	return p;
#endif
}

char *
xstrcatdup(const char *a, const char *b)
{
	return t_xstrcatdup(a, b);
}

char *
xstrcatdup(const char *a, const char *b, const char *c)
{
	return t_xstrcatdup(a, b, c);
}

char *
xstrcatdup(const char *a, const char *b, const char *c, const char *d)
{
	return t_xstrcatdup(a, b, c, d);
}
