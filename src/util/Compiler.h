// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef COMPILER_H
#define COMPILER_H

#define GCC_MAKE_VERSION(major, minor, patchlevel) ((major) * 10000 + (minor) * 100 + patchlevel)

#ifdef __GNUC__
#define GCC_VERSION GCC_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#else
#define GCC_VERSION 0
#endif

#ifdef __clang__
#  define CLANG_VERSION GCC_MAKE_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__)
#elif defined(__GNUC__)
#  define CLANG_VERSION 0
#endif

/**
 * Are we building with the specified version of gcc (not clang or any
 * other compiler) or newer?
 */
#define GCC_CHECK_VERSION(major, minor) \
	(CLANG_VERSION == 0 && \
	 GCC_VERSION >= GCC_MAKE_VERSION(major, minor, 0))

/**
 * Are we building with clang (any version) or at least the specified
 * gcc version?
 */
#define CLANG_OR_GCC_VERSION(major, minor) \
	(CLANG_VERSION > 0 || GCC_CHECK_VERSION(major, minor))

/**
 * Are we building with gcc (not clang or any other compiler) and a
 * version older than the specified one?
 */
#define GCC_OLDER_THAN(major, minor) \
	(GCC_VERSION > 0 && CLANG_VERSION == 0 && \
	 GCC_VERSION < GCC_MAKE_VERSION(major, minor, 0))

/**
 * Are we building with the specified version of clang or newer?
 */
#define CLANG_CHECK_VERSION(major, minor) \
	(CLANG_VERSION >= GCC_MAKE_VERSION(major, minor, 0))

#if CLANG_OR_GCC_VERSION(4,0)

/* GCC 4.x */

#define gcc_malloc __attribute__((malloc))
#define gcc_printf(a,b) __attribute__((format(printf, a, b)))
#define gcc_pure __attribute__((pure))

#define gcc_visibility_default __attribute__((visibility("default")))

#else

/* generic C compiler */

#define gcc_malloc
#define gcc_printf(a,b)
#define gcc_pure

#define gcc_visibility_default

#endif

#if GCC_CHECK_VERSION(7,0)
#define gcc_fallthrough __attribute__((fallthrough))
#elif CLANG_CHECK_VERSION(10,0) && defined(__cplusplus)
#define gcc_fallthrough [[fallthrough]]
#else
#define gcc_fallthrough
#endif

#ifndef __cplusplus
/* plain C99 has "restrict" */
#define gcc_restrict restrict
#elif CLANG_OR_GCC_VERSION(4,0)
/* "__restrict__" is a GCC extension for C++ */
#define gcc_restrict __restrict__
#else
/* disable it on other compilers */
#define gcc_restrict
#endif

#if defined(__GNUC__) || defined(__clang__)
#define gcc_unreachable() __builtin_unreachable()
#else
#define gcc_unreachable()
#endif

#endif
