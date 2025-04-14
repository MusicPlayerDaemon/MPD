// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/icu/Features.h" // for HAVE_ICU, HAVE_ICONV

#if (defined(HAVE_ICU) || defined(HAVE_ICONV)) && !defined(_WIN32)
#define HAVE_FS_CHARSET
#endif

#if !defined(HAVE_FS_CHARSET) && !defined(_WIN32)
/**
 * Is the filesystem character set hard-coded to UTF-8?
 */
#define FS_CHARSET_ALWAYS_UTF8
#endif
