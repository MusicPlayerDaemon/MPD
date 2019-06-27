/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "ConcatString.hxx"

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

template<typename... Args>
gcc_malloc gcc_nonnull_all
static inline char *
t_xstrcatdup(Args&&... args)
{
	constexpr size_t n = sizeof...(args);

	size_t lengths[n];
	const size_t total = FillLengths(lengths, args...);

	char *p = (char *)xalloc(total + 1);
	*StringCat(p, lengths, args...) = 0;
	return p;
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
