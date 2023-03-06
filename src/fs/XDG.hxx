// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_XDG_HXX
#define MPD_FS_XDG_HXX

// Use X Desktop guidelines where applicable
#if !defined(__APPLE__) && !defined(_WIN32) && !defined(ANDROID)
#define USE_XDG
#endif

#endif
