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

#ifndef MPD_ALLOC_HXX
#define MPD_ALLOC_HXX

#include "Compiler.h"

#include <stddef.h>

/**
 * Allocate memory.  Use free() to free it.
 *
 * This function never fails; in out-of-memory situations, it aborts
 * the process.
 */
gcc_malloc gcc_returns_nonnull
void *
xalloc(size_t size);

/**
 * Duplicate memory.  Use free() to free it.
 *
 * This function never fails; in out-of-memory situations, it aborts
 * the process.
 */
gcc_malloc gcc_returns_nonnull gcc_nonnull_all
void *
xmemdup(const void *s, size_t size);

/**
 * Duplicate a string.  Use free() to free it.
 *
 * This function never fails; in out-of-memory situations, it aborts
 * the process.
 */
gcc_malloc gcc_returns_nonnull gcc_nonnull_all
char *
xstrdup(const char *s);

/**
 * Duplicate a string.  Use free() to free it.
 *
 * This function never fails; in out-of-memory situations, it aborts
 * the process.
 */
gcc_malloc gcc_returns_nonnull gcc_nonnull_all
char *
xstrndup(const char *s, size_t n);

/**
 * Concatenate two strings, returning a new allocation.  Use free() to
 * free it.
 *
 * This function never fails; in out-of-memory situations, it aborts
 * the process.
 */
gcc_malloc gcc_returns_nonnull gcc_nonnull_all
char *
xstrcatdup(const char *a, const char *b);

gcc_malloc gcc_returns_nonnull gcc_nonnull_all
char *
xstrcatdup(const char *a, const char *b, const char *c);

gcc_malloc gcc_returns_nonnull gcc_nonnull_all
char *
xstrcatdup(const char *a, const char *b, const char *c, const char *d);

#endif
