// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_OPTIMIZE_FILTER_HXX
#define MPD_SONG_OPTIMIZE_FILTER_HXX

#include "ISongFilter.hxx"

class AndSongFilter;

void
OptimizeSongFilter(AndSongFilter &af) noexcept;

ISongFilterPtr
OptimizeSongFilter(ISongFilterPtr f) noexcept;

#endif
