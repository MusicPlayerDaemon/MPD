/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#ifndef MPD_UPDATE_REMOVE_H
#define MPD_UPDATE_REMOVE_H

#include "check.h"

struct song;

void
update_remove_global_init(void);

void
update_remove_global_finish(void);

/**
 * Sends a signal to the main thread which will in turn remove the
 * song: from the sticker database and from the playlist.  This
 * serialized access is implemented to avoid excessive locking.
 */
void
update_remove_song(const struct song *song);

#endif
