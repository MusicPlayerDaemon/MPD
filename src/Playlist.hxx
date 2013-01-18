/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_PLAYLIST_HXX
#define MPD_PLAYLIST_HXX

#include "Queue.hxx"
#include "playlist_error.h"

#include <stdbool.h>

struct player_control;

struct playlist {
	/**
	 * The song queue - it contains the "real" playlist.
	 */
	struct queue queue;

	/**
	 * This value is true if the player is currently playing (or
	 * should be playing).
	 */
	bool playing;

	/**
	 * If true, then any error is fatal; if false, MPD will
	 * attempt to play the next song on non-fatal errors.  During
	 * seeking, this flag is set.
	 */
	bool stop_on_error;

	/**
	 * Number of errors since playback was started.  If this
	 * number exceeds the length of the playlist, MPD gives up,
	 * because all songs have been tried.
	 */
	unsigned error_count;

	/**
	 * The "current song pointer".  This is the song which is
	 * played when we get the "play" command.  It is also the song
	 * which is currently being played.
	 */
	int current;

	/**
	 * The "next" song to be played, when the current one
	 * finishes.  The decoder thread may start decoding and
	 * buffering it, while the "current" song is still playing.
	 *
	 * This variable is only valid if #playing is true.
	 */
	int queued;

	playlist(unsigned max_length)
		:queue(max_length), playing(false), current(-1), queued(-1) {
	}

	~playlist() {
	}

	uint32_t GetVersion() const {
		return queue.version;
	}

	unsigned GetLength() const {
		return queue.GetLength();
	}

	unsigned PositionToId(unsigned position) const {
		return queue.PositionToId(position);
	}

	gcc_pure
	int GetCurrentPosition() const;

	gcc_pure
	int GetNextPosition() const;

	/**
	 * Returns the song object which is currently queued.  Returns
	 * none if there is none (yet?) or if MPD isn't playing.
	 */
	gcc_pure
	const struct song *GetQueuedSong() const;

	/**
	 * This is the "PLAYLIST" event handler.  It is invoked by the
	 * player thread whenever it requests a new queued song, or
	 * when it exits.
	 */
	void SyncWithPlayer(player_control &pc);

protected:
	/**
	 * Called by all editing methods after a modification.
	 * Updates the queue version and emits #IDLE_PLAYLIST.
	 */
	void OnModified();

	/**
	 * Updates the "queued song".  Calculates the next song
	 * according to the current one (if MPD isn't playing, it
	 * takes the first song), and queues this song.  Clears the
	 * old queued song if there was one.
	 *
	 * @param prev the song which was previously queued, as
	 * determined by playlist_get_queued_song()
	 */
	void UpdateQueuedSong(player_control &pc, const song *prev);

public:
	void Clear(player_control &pc);

	void TagChanged();

	void FullIncrementVersions();

	enum playlist_result AppendSong(player_control &pc,
					struct song *song,
					unsigned *added_id=nullptr);

	/**
	 * Appends a local file (outside the music database) to the
	 * playlist.
	 *
	 * Note: the caller is responsible for checking permissions.
	 */
	enum playlist_result AppendFile(player_control &pc,
					const char *path_utf8,
					unsigned *added_id=nullptr);

	enum playlist_result AppendURI(player_control &pc,
				       const char *uri_utf8,
				       unsigned *added_id=nullptr);

protected:
	void DeleteInternal(player_control &pc,
			    unsigned song, const struct song **queued_p);

public:
	enum playlist_result DeletePosition(player_control &pc,
					    unsigned position);

	enum playlist_result DeleteOrder(player_control &pc,
					 unsigned order) {
		return DeletePosition(pc, queue.OrderToPosition(order));
	}

	enum playlist_result DeleteId(player_control &pc, unsigned id);

	/**
	 * Deletes a range of songs from the playlist.
	 *
	 * @param start the position of the first song to delete
	 * @param end the position after the last song to delete
	 */
	enum playlist_result DeleteRange(player_control &pc,
					 unsigned start, unsigned end);

	void DeleteSong(player_control &pc, const song &song);

	void Shuffle(player_control &pc, unsigned start, unsigned end);

	enum playlist_result MoveRange(player_control &pc,
				       unsigned start, unsigned end, int to);

	enum playlist_result MoveId(player_control &pc, unsigned id, int to);

	enum playlist_result SwapPositions(player_control &pc,
					   unsigned song1, unsigned song2);

	enum playlist_result SwapIds(player_control &pc,
				     unsigned id1, unsigned id2);

	enum playlist_result SetPriorityRange(player_control &pc,
					      unsigned start_position,
					      unsigned end_position,
					      uint8_t priority);

	enum playlist_result SetPriorityId(player_control &pc,
					   unsigned song_id, uint8_t priority);

	void Stop(player_control &pc);

	enum playlist_result PlayPosition(player_control &pc, int position);

	void PlayOrder(player_control &pc, int order);

	enum playlist_result PlayId(player_control &pc, int id);

	void PlayNext(player_control &pc);

	void PlayPrevious(player_control &pc);

	enum playlist_result SeekSongPosition(player_control &pc,
					      unsigned song_position,
					      float seek_time);

	enum playlist_result SeekSongId(player_control &pc,
					unsigned song_id, float seek_time);

	/**
	 * Seek within the current song.  Fails if MPD is not currently
	 * playing.
	 *
	 * @param time the time in seconds
	 * @param relative if true, then the specified time is relative to the
	 * current position
	 */
	enum playlist_result SeekCurrent(player_control &pc,
					 float seek_time, bool relative);

	bool GetRepeat() const {
		return queue.repeat;
	}

	void SetRepeat(player_control &pc, bool new_value);

	bool GetRandom() const {
		return queue.random;
	}

	void SetRandom(player_control &pc, bool new_value);

	bool GetSingle() const {
		return queue.single;
	}

	void SetSingle(player_control &pc, bool new_value);

	bool GetConsume() const {
		return queue.consume;
	}

	void SetConsume(bool new_value);
};

#endif
