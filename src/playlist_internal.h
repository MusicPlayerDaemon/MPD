/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Internal header for the components of the playlist code.
 *
 */

#ifndef PLAYLIST_INTERNAL_H
#define PLAYLIST_INTERNAL_H

#include "playlist.h"

/**
 * Returns the song object which is currently queued.  Returns none if
 * there is none (yet?) or if MPD isn't playing.
 */
const struct song *
playlist_get_queued_song(struct playlist *playlist);

/**
 * Updates the "queued song".  Calculates the next song according to
 * the current one (if MPD isn't playing, it takes the first song),
 * and queues this song.  Clears the old queued song if there was one.
 *
 * @param prev the song which was previously queued, as determined by
 * playlist_get_queued_song()
 */
void
playlist_update_queued_song(struct playlist *playlist,
			    const struct song *prev);

void
playPlaylistOrderNumber(struct playlist *playlist, int orderNum);

#endif
