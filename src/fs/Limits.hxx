// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_LIMITS_HXX
#define MPD_FS_LIMITS_HXX

#include <climits>
#include <cstddef>

#if defined(_WIN32)
static constexpr size_t MPD_PATH_MAX = 260;
#elif defined(MAXPATHLEN)
static constexpr size_t MPD_PATH_MAX = MAXPATHLEN;
#elif defined(PATH_MAX)
static constexpr size_t MPD_PATH_MAX = PATH_MAX;
#else
static constexpr size_t MPD_PATH_MAX = 256;
#endif

#endif
