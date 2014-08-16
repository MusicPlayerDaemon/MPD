/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

/* \file
 *
 * The player thread controls the playback.  It acts as a bridge
 * between the decoder thread and the output thread(s): it receives
 * #MusicChunk objects from the decoder, optionally mixes them
 * (cross-fading), applies software volume, and sends them to the
 * audio outputs via audio_output_all_play().
 *
 * It is controlled by the main thread (the playlist code), see
 * PlayerControl.hxx.  The playlist enqueues new songs into the player
 * thread and sends it commands.
 *
 * The player thread itself does not do any I/O.  It synchronizes with
 * other threads via #GMutex and #GCond objects, and passes
 * #MusicChunk instances around in #MusicPipe objects.
 */

#ifndef MPD_PLAYER_THREAD_HXX
#define MPD_PLAYER_THREAD_HXX

struct PlayerControl;

void
StartPlayerThread(PlayerControl &pc);

#endif
