// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * Portability macros for opening files.
 */

#ifndef MPD_OPEN_H
#define MPD_OPEN_H

#include <fcntl.h>

/* On Windows, files are opened in "text" mode by default, and the C
   library will mangle data being read/written; this must be switched
   off by specifying the proprietary "O_BINARY" flag.  That sucks! */
#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif

#endif
