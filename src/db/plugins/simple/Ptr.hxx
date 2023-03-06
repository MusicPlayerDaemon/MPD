// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_PTR_HXX
#define MPD_SONG_PTR_HXX

#include <memory>

struct Song;

using SongPtr = std::unique_ptr<Song>;

#endif
