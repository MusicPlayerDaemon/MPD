/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "queue/Playlist.hxx"
#include "output/MultipleOutputs.hxx"
#include "mixer/Listener.hxx"
#include "player/Control.hxx"
#include "player/Listener.hxx"
#include "Chrono.hxx"
#include "Compiler.h"

struct Instance;
class MultipleOutputs;
class SongLoader;

/**
 * A partition of the Music Player Daemon.  It is a separate unit with
 * a playlist, a player, outputs etc.
 */
struct Partition final : private PlayerListener, private MixerListener {
	Instance &instance;

	struct playlist playlist;

	MultipleOutputs outputs;

	PlayerControl pc;

	Partition(Instance &_instance,
		  unsigned max_length,
		  unsigned buffer_chunks,
		  unsigned buffered_before_play);

	void EmitIdle(unsigned mask);

	void ClearQueue() {
		playlist.Clear(pc);
	}

	unsigned AppendURI(const SongLoader &loader,
			   const char *uri_utf8,
			   Error &error) {
		return playlist.AppendURI(pc, loader, uri_utf8, error);
	}

	void DeletePosition(unsigned position) {
		playlist.DeletePosition(pc, position);
	}

	void DeleteId(unsigned id) {
		playlist.DeleteId(pc, id);
	}

	/**
	 * Deletes a range of songs from the playlist.
	 *
	 * @param start the position of the first song to delete
	 * @param end the position after the last song to delete
	 */
	void DeleteRange(unsigned start, unsigned end) {
		playlist.DeleteRange(pc, start, end);
	}

#ifdef ENABLE_DATABASE

	void DeleteSong(const char *uri) {
		playlist.DeleteSong(pc, uri);
	}

#endif

	void Shuffle(unsigned start, unsigned end) {
		playlist.Shuffle(pc, start, end);
	}

	void MoveRange(unsigned start, unsigned end, int to) {
		playlist.MoveRange(pc, start, end, to);
	}

	void MoveId(unsigned id, int to) {
		playlist.MoveId(pc, id, to);
	}

	void SwapPositions(unsigned song1, unsigned song2) {
		playlist.SwapPositions(pc, song1, song2);
	}

	void SwapIds(unsigned id1, unsigned id2) {
		playlist.SwapIds(pc, id1, id2);
	}

	void SetPriorityRange(unsigned start_position, unsigned end_position,
			      uint8_t priority) {
		playlist.SetPriorityRange(pc, start_position, end_position,
					  priority);
	}

	void SetPriorityId(unsigned song_id, uint8_t priority) {
		playlist.SetPriorityId(pc, song_id, priority);
	}

	void Stop() {
		playlist.Stop(pc);
	}

	bool PlayPosition(int position, Error &error) {
		return playlist.PlayPosition(pc, position, error);
	}

	bool PlayId(int id, Error &error) {
		return playlist.PlayId(pc, id, error);
	}

	bool PlayNext(Error &error) {
		return playlist.PlayNext(pc, error);
	}

	bool PlayPrevious(Error &error) {
		return playlist.PlayPrevious(pc, error);
	}

	bool SeekSongPosition(unsigned song_position,
			      SongTime seek_time, Error &error) {
		return playlist.SeekSongPosition(pc, song_position, seek_time,
						 error);
	}

	bool SeekSongId(unsigned song_id, SongTime seek_time, Error &error) {
		return playlist.SeekSongId(pc, song_id, seek_time, error);
	}

	bool SeekCurrent(SignedSongTime seek_time, bool relative,
			 Error &error) {
		return playlist.SeekCurrent(pc, seek_time, relative, error);
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

#ifdef ENABLE_DATABASE
	/**
	 * Returns the global #Database instance.  May return nullptr
	 * if this MPD configuration has no database (no
	 * music_directory was configured).
	 */
	const Database *GetDatabase(Error &error) const;

	/**
	 * The database has been modified.  Propagate the change to
	 * all subsystems.
	 */
	void DatabaseModified(const Database &db);
#endif

	/**
	 * A tag in the play queue has been modified by the player
	 * thread.  Propagate the change to all subsystems.
	 */
	void TagModified();

	/**
	 * Synchronize the player with the play queue.
	 */
	void SyncWithPlayer();

private:
	/* virtual methods from class PlayerListener */
	virtual void OnPlayerSync() override;
	virtual void OnPlayerTagModified() override;

	/* virtual methods from class MixerListener */
	virtual void OnMixerVolumeChanged(Mixer &mixer, int volume) override;
};

#endif
