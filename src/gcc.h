/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_GCC_H
#define MPD_GCC_H

/* this allows us to take advantage of special gcc features while still
 * allowing other compilers to compile:
 *
 * example taken from: http://rlove.org/log/2005102601
 */

#if defined(__GNUC__) && (__GNUC__ >= 3)
#  define mpd_const		__attribute__ ((const))
#  define mpd_deprecated	__attribute__ ((deprecated))
#  define mpd_malloc		__attribute__ ((malloc))
#  define mpd_must_check	__attribute__ ((warn_unused_result))
#  define mpd_noreturn		__attribute__ ((noreturn))
#  define mpd_packed		__attribute__ ((packed))
/* these are very useful for type checking */
#  define mpd_printf		__attribute__ ((format(printf,1,2)))
#  define mpd_fprintf		__attribute__ ((format(printf,2,3)))
#  define mpd_fprintf_		__attribute__ ((format(printf,3,4)))
#  define mpd_fprintf__		__attribute__ ((format(printf,4,5)))
#  define mpd_pure		__attribute__ ((pure))
#  define mpd_scanf		__attribute__ ((format(scanf,1,2)))
#  define mpd_unused		__attribute__ ((unused))
#  define mpd_used		__attribute__ ((used))
/* #  define inline	inline __attribute__ ((always_inline)) */
#  define mpd_noinline		__attribute__ ((noinline))
#  define mpd_likely(x)		__builtin_expect (!!(x), 1)
#  define mpd_unlikely(x)	__builtin_expect (!!(x), 0)
#else
#  define mpd_const
#  define mpd_deprecated
#  define mpd_malloc
#  define mpd_must_check
#  define mpd_noreturn
#  define mpd_packed
#  define mpd_printf
#  define mpd_fprintf
#  define mpd_fprintf_
#  define mpd_fprintf__
#  define mpd_pure
#  define mpd_scanf
#  define mpd_unused
#  define mpd_used
/* #  define inline */
#  define mpd_noinline
#  define mpd_likely(x)		(x)
#  define mpd_unlikely(x)	(x)
#endif

#endif /* MPD_GCC_H */
