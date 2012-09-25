/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_GCC_H
#define MPD_GCC_H

#define GCC_CHECK_VERSION(major, minor) \
	(defined(__GNUC__) && \
	 (__GNUC__ > (major) || \
	  (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))))

/* this allows us to take advantage of special gcc features while still
 * allowing other compilers to compile:
 *
 * example taken from: http://rlove.org/log/2005102601
 */

#if GCC_CHECK_VERSION(3,0)
#  define gcc_const __attribute__((const))
#  define gcc_pure __attribute__((pure))
#  define gcc_malloc __attribute__((malloc))
#  define gcc_must_check	__attribute__ ((warn_unused_result))
#  define gcc_packed		__attribute__ ((packed))
/* these are very useful for type checking */
#  define gcc_printf		__attribute__ ((format(printf,1,2)))
#  define gcc_fprintf		__attribute__ ((format(printf,2,3)))
#  define gcc_fprintf_		__attribute__ ((format(printf,3,4)))
#  define gcc_fprintf__		__attribute__ ((format(printf,4,5)))
#  define gcc_scanf		__attribute__ ((format(scanf,1,2)))
#  define gcc_used		__attribute__ ((used))
#  define gcc_unused __attribute__((unused))
#  define gcc_warn_unused_result __attribute__((warn_unused_result))
/* #  define inline	inline __attribute__ ((always_inline)) */
#  define gcc_noinline		__attribute__ ((noinline))
#  define gcc_nonnull(...) __attribute__((nonnull(__VA_ARGS__)))
#  define gcc_nonnull_all __attribute__((nonnull))

#  define gcc_likely(x) __builtin_expect (!!(x), 1)
#  define gcc_unlikely(x) __builtin_expect (!!(x), 0)

#else
#  define gcc_unused
#  define gcc_const
#  define gcc_pure
#  define gcc_malloc
#  define gcc_must_check
#  define gcc_packed
#  define gcc_printf
#  define gcc_fprintf
#  define gcc_fprintf_
#  define gcc_fprintf__
#  define gcc_scanf
#  define gcc_used
#  define gcc_unused
#  define gcc_warn_unused_result
/* #  define inline */
#  define gcc_noinline
#  define gcc_nonnull(...)
#  define gcc_nonnull_all

#  define gcc_likely(x) (x)
#  define gcc_unlikely(x) (x)

#endif

#ifdef __cplusplus

#if !defined(__clang__) && defined(__GNUC__) && !GCC_CHECK_VERSION(4,6)
#error Your gcc version is too old.  MPD requires gcc 4.6 or newer.
#endif

/* support for C++11 "override" was added in gcc 4.7 */
#if !defined(__clang__) && defined(__GNUC__) && !GCC_CHECK_VERSION(4,7)
#define override
#endif

#endif

#endif /* MPD_GCC_H */
