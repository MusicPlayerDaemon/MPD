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

/*
 * Compatibility header for GLib before 2.16.
 */

#ifndef MPD_TEST_GLIB_COMPAT_H
#define MPD_TEST_GLIB_COMPAT_H

#include <glib.h>

#if !GLIB_CHECK_VERSION(2,16,0)

#define g_assert_cmpint(n1, cmp, n2) g_assert((n1) cmp (n2))

static void (*test_functions[256])(void);
static unsigned num_test_functions;

static inline void
g_test_init(G_GNUC_UNUSED int *argc, G_GNUC_UNUSED char ***argv, ...)
{
}

static inline void
g_test_add_func(G_GNUC_UNUSED const char *testpath, void (test_funcvoid)(void))
{
	test_functions[num_test_functions++] = test_funcvoid;
}

static inline int
g_test_run(void)
{
	for (unsigned i = 0; i < num_test_functions; ++i)
		test_functions[i]();
	return 0;
}

#endif /* !2.16 */

#endif
