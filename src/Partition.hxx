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

#ifndef MPD_PARTITION_HXX
#define MPD_PARTITION_HXX

#include "Playlist.hxx"
#include "output/MultipleOutputs.hxx"
#include "PlayerControl.hxx"

struct Instance;
class MultipleOutputs;

/**
 * A partition of the Music Player Daemon.  It is a separate unit with
 * a playlist, a player, outputs etc.
 */
struct Partition {
	Instance &instance;

	struct playlist playlist;

	MultipleOutputs outputs;

	PlayerControl pc;

	Partition(Instance &_instance,
		  unsigned max_length,
		  unsigned buffer_chunks,
		  unsigned buffered_before_play)
		:instance(_instance), playlist(max_length),
		 pc(outputs, buffer_chunks, buffered_before_play) {}

	void ClearQueue() {
		playlist.Clear(pc);
	}

	PlaylistResult AppendFile(const char *path_utf8,
				  unsigned *added_id=nullptr) {
		return playlist.AppendFile(pc, path_utf8, added_id);
	}

	PlaylistResult AppendURI(const char *uri_utf8,
				 unsigned *added_id=nullptr) {
		return playlist.AppendURI(pc, uri_utf8, added_id);
	}

	PlaylistResult DeletePosition(unsigned position) {
		return playlist.DeletePosition(pc, position);
	}

	PlaylistResult DeleteId(unsigned id) {
		return playlist.DeleteId(pc, id);
	}

	/**
	 * Deletes a range of songs from the playlist.
	 *
	 * @param start the position of the first song to delete
	 * @param end the position after the last song to delete
	 */
	PlaylistResult DeleteRange(unsigned start, unsigned end) {
		return playlist.DeleteRange(pc, start, end);
	}

	void DeleteSong(const char *uri) {
		playlist.DeleteSong(pc, uri);
	}

	void Shuffle(unsigned start, unsigned end) {
		playlist.Shuffle(pc, start, end);
	}

	PlaylistResult MoveRange(unsigned start, unsigned end, int to) {
		return playlist.MoveRange(pc, start, end, to);
	}

	PlaylistResult MoveId(unsigned id, int to) {
		return playlist.MoveId(pc, id, to);
	}

	PlaylistResult SwapPositions(unsigned song1, unsigned song2) {
		return playlist.SwapPositions(pc, song1, song2);
	}

	PlaylistResult SwapIds(unsigned id1, unsigned id2) {
		return playlist.SwapIds(pc, id1, id2);
	}

	PlaylistResult SetPriorityRange(unsigned start_position,
					unsigned end_position,
					uint8_t priority) {
		return playlist.SetPriorityRange(pc,
						 start_position, end_position,
						 priority);
	}

	PlaylistResult SetPriorityId(unsigned song_id,
				     uint8_t priority) {
		return playlist.SetPriorityId(pc, song_id, priority);
	}

	void Stop() {
		playlist.Stop(pc);
	}

	PlaylistResult PlayPosition(int position) {
		return playlist.PlayPosition(pc, position);
	}

	PlaylistResult PlayId(int id) {
		return playlist.PlayId(pc, id);
	}

	void PlayNext() {
		return playlist.PlayNext(pc);
	}

	void PlayPrevious() {
		return playlist.PlayPrevious(pc);
	}

	PlaylistResult SeekSongPosition(unsigned song_position,
					float seek_time) {
		return playlist.SeekSongPosition(pc, song_position, seek_time);
	}

	PlaylistResult SeekSongId(unsigned song_id, float seek_time) {
		return playlist.SeekSongId(pc, song_id, seek_time);
	}

	PlaylistResult SeekCurrent(float seek_time, bool relative) {
		return playlist.SeekCurrent(pc, seek_time, relative);
	}

	void SetRepeat(bool new_value) {
		playlist.SetRepeat(pc, new_value);
	}

	bool GetRandom() const {
		return playlist.GetRandom();
	}

	void SetRandom(bool new_value) {
		playlist.SetRandom(pc, new_value);
	}

	void SetSingle(bool new_value) {
		playlist.SetSingle(pc, new_value);
	}

	void SetConsume(bool new_value) {
		playlist.SetConsume(new_value);
	}

	/**
	 * The database has been modified.  Propagate the change to
	 * all subsystems.
	 */
	void DatabaseModified();

	/**
	 * A tag in the play queue has been modified by the player
	 * thread.  Propagate the change to all subsystems.
	 */
	void TagModified();

	/**
	 * Synchronize the player with the play queue.
	 */
	void SyncWithPlayer();
};

#endif
