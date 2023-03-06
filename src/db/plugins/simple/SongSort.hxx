// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_SORT_HXX
#define MPD_SONG_SORT_HXX

#include "util/IntrusiveList.hxx"

struct Song;

void
song_list_sort(IntrusiveList<Song> &songs) noexcept;

#endif
