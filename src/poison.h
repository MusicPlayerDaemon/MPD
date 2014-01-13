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

#ifndef MPD_POISON_H
#define MPD_POISON_H

#include "check.h"

#ifndef NDEBUG

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include <string.h>
#endif

/**
 * Poisons the specified memory area and marks it as "not accessible".
 *
 * @param p pointer to the memory area
 * @param length number of bytes to poison
 */
static inline void
poison_noaccess(void *p, size_t length)
{
#ifdef NDEBUG
	(void)p;
	(void)length;
#else
	memset(p, 0x01, length);

#ifdef HAVE_VALGRIND_MEMCHECK_H
	(void)VALGRIND_MAKE_MEM_NOACCESS(p, length);
#endif
#endif
}

/**
 * Poisons the specified memory area and marks it as "not defined".
 *
 * @param p pointer to the memory area
 * @param length number of bytes to poison
 */
static inline void
poison_undefined(void *p, size_t length)
{
#ifdef NDEBUG
	(void)p;
	(void)length;
#else
	memset(p, 0x02, length);

#ifdef HAVE_VALGRIND_MEMCHECK_H
	(void)VALGRIND_MAKE_MEM_UNDEFINED(p, length);
#endif
#endif
}


#endif
