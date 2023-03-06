// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_FEATURES_HXX
#define MPD_FS_FEATURES_HXX

#include "config.h"

#if (defined(HAVE_ICU) || defined(HAVE_ICONV)) && !defined(_WIN32)
#define HAVE_FS_CHARSET
#endif

#if !defined(HAVE_FS_CHARSET) && !defined(_WIN32)
/**
 * Is the filesystem character set hard-coded to UTF-8?
 */
#define FS_CHARSET_ALWAYS_UTF8
#endif

#endif
