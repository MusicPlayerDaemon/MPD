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

#ifndef COMPILER_H
#define COMPILER_H

#define GCC_MAKE_VERSION(major, minor, patchlevel) ((major) * 10000 + (minor) * 100 + patchlevel)

#ifdef __GNUC__
#define GCC_VERSION GCC_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#else
#define GCC_VERSION 0
#endif

#define GCC_CHECK_VERSION(major, minor) \
	(defined(__GNUC__) && GCC_VERSION >= GCC_MAKE_VERSION(major, minor, 0))

/**
 * Are we building with gcc (not clang or any other compiler) and a
 * version older than the specified one?
 */
#define GCC_OLDER_THAN(major, minor) \
	(defined(__GNUC__) && !defined(__clang__) && \
	 GCC_VERSION < GCC_MAKE_VERSION(major, minor, 0))

#ifdef __clang__
#  define CLANG_VERSION GCC_MAKE_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__)
#  if __clang_major__ < 3
#    error Sorry, your clang version is too old.  You need at least version 3.1.
#  endif
#elif defined(__GNUC__)
#  if GCC_OLDER_THAN(4,6)
#    error Sorry, your gcc version is too old.  You need at least version 4.6.
#  endif
#else
#  warning Untested compiler.  Use at your own risk!
#endif

/**
 * Are we building with the specified version of clang or newer?
 */
#define CLANG_CHECK_VERSION(major, minor) \
	(defined(__clang__) && \
	 CLANG_VERSION >= GCC_MAKE_VERSION(major, minor, 0))

#if GCC_CHECK_VERSION(4,0)

/* GCC 4.x */

#define gcc_const __attribute__((const))
#define gcc_deprecated __attribute__((deprecated))
#define gcc_may_alias __attribute__((may_alias))
#define gcc_malloc __attribute__((malloc))
#define gcc_noreturn __attribute__((noreturn))
#define gcc_packed __attribute__((packed))
#define gcc_printf(a,b) __attribute__((format(printf, a, b)))
#define gcc_pure __attribute__((pure))
#define gcc_sentinel __attribute__((sentinel))
#define gcc_unused __attribute__((unused))
#define gcc_warn_unused_result __attribute__((warn_unused_result))

#define gcc_nonnull(...) __attribute__((nonnull(__VA_ARGS__)))
#define gcc_nonnull_all __attribute__((nonnull))

#define gcc_likely(x) __builtin_expect (!!(x), 1)
#define gcc_unlikely(x) __builtin_expect (!!(x), 0)

#define gcc_aligned(n) __attribute__((aligned(n)))

#define gcc_visibility_hidden __attribute__((visibility("hidden")))
#define gcc_visibility_default __attribute__((visibility("default")))

#define gcc_always_inline __attribute__((always_inline))

#else

/* generic C compiler */

#define gcc_const
#define gcc_deprecated
#define gcc_may_alias
#define gcc_malloc
#define gcc_noreturn
#define gcc_packed
#define gcc_printf(a,b)
#define gcc_pure
#define gcc_sentinel
#define gcc_unused
#define gcc_warn_unused_result

#define gcc_nonnull(...)
#define gcc_nonnull_all

#define gcc_likely(x) (x)
#define gcc_unlikely(x) (x)

#define gcc_aligned(n)

#define gcc_visibility_hidden
#define gcc_visibility_default

#define gcc_always_inline inline

#endif

#if GCC_CHECK_VERSION(4,3)

#define gcc_hot __attribute__((hot))
#define gcc_cold __attribute__((cold))

#else /* ! GCC_UNUSED >= 40300 */

#define gcc_hot
#define gcc_cold

#endif /* ! GCC_UNUSED >= 40300 */

#if GCC_CHECK_VERSION(4,6) && !defined(__clang__)
#define gcc_flatten __attribute__((flatten))
#else
#define gcc_flatten
#endif

#ifndef __cplusplus
/* plain C99 has "restrict" */
#define gcc_restrict restrict
#elif GCC_CHECK_VERSION(4,0)
/* "__restrict__" is a GCC extension for C++ */
#define gcc_restrict __restrict__
#else
/* disable it on other compilers */
#define gcc_restrict
#endif

/* C++11 features */

#if defined(__cplusplus)

/* support for C++11 "override" was added in gcc 4.7 */
#if GCC_OLDER_THAN(4,7)
#define override
#define final
#endif

#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
#define gcc_alignas(T, fallback) alignas(T)
#else
#define gcc_alignas(T, fallback) gcc_aligned(fallback)
#endif

#endif

#ifndef __has_feature
  // define dummy macro for non-clang compilers
  #define __has_feature(x) 0
#endif

#if __has_feature(attribute_unused_on_fields)
#define gcc_unused_field gcc_unused
#else
#define gcc_unused_field
#endif

#if defined(__GNUC__) || defined(__clang__)
#define gcc_unreachable() __builtin_unreachable()
#else
#define gcc_unreachable()
#endif

#endif
