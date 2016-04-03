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

#ifndef MPD_PLAYLIST_HXX
#define MPD_PLAYLIST_HXX

#include "queue/Queue.hxx"

enum TagType : uint8_t;
struct PlayerControl;
class DetachedSong;
class Database;
class Error;
class SongLoader;
class SongTime;
class SignedSongTime;
class QueueListener;

struct playlist {
	/**
	 * The song queue - it contains the "real" playlist.
	 */
	Queue queue;

	QueueListener &listener;

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
	 * If true, then a bulk edit has been initiated by
	 * BeginBulk(), and UpdateQueuedSong() and OnModified() will
	 * be postponed until CommitBulk()
	 */
	bool bulk_edit;

	/**
	 * Has the queue been modified during bulk edit mode?
	 */
	bool bulk_modified;

	/**
	 * Number of errors since playback was started.  If this
	 * number exceeds the length of the playlist, MPD gives up,
	 * because all songs have been tried.
	 */
	unsigned error_count;

	/**
	 * The "current song pointer" (the order number).  This is the
	 * song which is played when we get the "play" command.  It is
	 * also the song which is currently being played.
	 */
	int current;

	/**
	 * The "next" song to be played (the order number), when the
	 * current one finishes.  The decoder thread may start
	 * decoding and buffering it, while the "current" song is
	 * still playing.
	 *
	 * This variable is only valid if #playing is true.
	 */
	int queued;

	playlist(unsigned max_length,
		 QueueListener &_listener)
		:queue(max_length),
		 listener(_listener),
		 playing(false),
		 bulk_edit(false),
		 current(-1), queued(-1) {
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
	const DetachedSong *GetQueuedSong() const;

	/**
	 * This is the "PLAYLIST" event handler.  It is invoked by the
	 * player thread whenever it requests a new queued song, or
	 * when it exits.
	 */
	void SyncWithPlayer(PlayerControl &pc);

protected:
	/**
	 * Called by all editing methods after a modification.
	 * Updates the queue version and invokes
	 * QueueListener::OnQueueModified().
	 */
	void OnModified();

	/**
	 * Called when playback of a new song starts.  Unlike
	 * QueuedSongStarted(), this also gets called when the user
	 * manually switches to another song.  It may be used for
	 * playlist fixups.
	 *
	 * The song being started is specified by the #current
	 * attribute.
	 */
	void SongStarted();

	/**
	 * Updates the "queued song".  Calculates the next song
	 * according to the current one (if MPD isn't playing, it
	 * takes the first song), and queues this song.  Clears the
	 * old queued song if there was one.
	 *
	 * @param prev the song which was previously queued, as
	 * determined by playlist_get_queued_song()
	 */
	void UpdateQueuedSong(PlayerControl &pc, const DetachedSong *prev);

	/**
	 * Queue a song, addressed by its order number.
	 */
	void QueueSongOrder(PlayerControl &pc, unsigned order);

	/**
	 * Called when the player thread has started playing the
	 * "queued" song, i.e. it has switched from one song to the
	 * next automatically.
	 */
	void QueuedSongStarted(PlayerControl &pc);

	/**
	 * The player has stopped for some reason.  Check the error,
	 * and decide whether to re-start playback.
	 */
	void ResumePlayback(PlayerControl &pc);

public:
	void BeginBulk();
	void CommitBulk(PlayerControl &pc);

	void Clear(PlayerControl &pc);

	/**
	 * A tag in the play queue has been modified by the player
	 * thread.  Apply the given song's tag to the current song if
	 * the song matches.
	 */
	void TagModified(DetachedSong &&song);

#ifdef ENABLE_DATABASE
	/**
	 * The database has been modified.  Pull all updates.
	 */
	void DatabaseModified(const Database &db);
#endif

	/**
	 * Throws PlaylistError if the queue would be too large.
	 *
	 * @return the new song id
	 */
	unsigned AppendSong(PlayerControl &pc, DetachedSong &&song);

	/**
	 * @return the new song id or 0 on error
	 */
	unsigned AppendURI(PlayerControl &pc,
			   const SongLoader &loader,
			   const char *uri_utf8,
			   Error &error);

protected:
	void DeleteInternal(PlayerControl &pc,
			    unsigned song, const DetachedSong **queued_p);

public:
	void DeletePosition(PlayerControl &pc, unsigned position);

	void DeleteOrder(PlayerControl &pc, unsigned order) {
		DeletePosition(pc, queue.OrderToPosition(order));
	}

	void DeleteId(PlayerControl &pc, unsigned id);

	/**
	 * Deletes a range of songs from the playlist.
	 *
	 * @param start the position of the first song to delete
	 * @param end the position after the last song to delete
	 */
	void DeleteRange(PlayerControl &pc, unsigned start, unsigned end);

	/**
	 * Mark the given song as "stale", i.e. as not being available
	 * anymore.  This gets called when a song is removed from the
	 * database.  The method attempts to remove all instances of
	 * this song from the queue.
	 */
	void StaleSong(PlayerControl &pc, const char *uri);

	void Shuffle(PlayerControl &pc, unsigned start, unsigned end);

	void MoveRange(PlayerControl &pc, unsigned start,
		       unsigned end, int to);

	void MoveId(PlayerControl &pc, unsigned id, int to);

	void SwapPositions(PlayerControl &pc, unsigned song1, unsigned song2);

	void SwapIds(PlayerControl &pc, unsigned id1, unsigned id2);

	void SetPriorityRange(PlayerControl &pc,
			      unsigned start_position, unsigned end_position,
			      uint8_t priority);

	void SetPriorityId(PlayerControl &pc,
			   unsigned song_id, uint8_t priority);

	/**
	 * Sets the start_time and end_time attributes on the song
	 * with the specified id.
	 */
	void SetSongIdRange(PlayerControl &pc, unsigned id,
			    SongTime start, SongTime end);

	void AddSongIdTag(unsigned id, TagType tag_type, const char *value);
	void ClearSongIdTag(unsigned id, TagType tag_type);

	void Stop(PlayerControl &pc);

	bool PlayPosition(PlayerControl &pc, int position, Error &error);

	bool PlayOrder(PlayerControl &pc, unsigned order, Error &error);

	bool PlayId(PlayerControl &pc, int id, Error &error);

	bool PlayNext(PlayerControl &pc, Error &error);

	bool PlayPrevious(PlayerControl &pc, Error &error);

	bool SeekSongOrder(PlayerControl &pc,
			   unsigned song_order,
			   SongTime seek_time,
			   Error &error);

	bool SeekSongPosition(PlayerControl &pc,
			      unsigned sonag_position,
			      SongTime seek_time,
			      Error &error);

	bool SeekSongId(PlayerControl &pc,
			unsigned song_id, SongTime seek_time,
			Error &error);

	/**
	 * Seek within the current song.  Fails if MPD is not currently
	 * playing.
	 *
	 * @param seek_time the time
	 * @param relative if true, then the specified time is relative to the
	 * current position
	 */
	bool SeekCurrent(PlayerControl &pc,
			 SignedSongTime seek_time, bool relative,
			 Error &error);

	bool GetRepeat() const {
		return queue.repeat;
	}

	void SetRepeat(PlayerControl &pc, bool new_value);

	bool GetRandom() const {
		return queue.random;
	}

	void SetRandom(PlayerControl &pc, bool new_value);

	bool GetSingle() const {
		return queue.single;
	}

	void SetSingle(PlayerControl &pc, bool new_value);

	bool GetConsume() const {
		return queue.consume;
	}

	void SetConsume(bool new_value);
};

#endif
