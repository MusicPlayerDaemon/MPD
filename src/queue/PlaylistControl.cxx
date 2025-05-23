// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Functions for controlling playback on the playlist level.
 *
 */

#include "Playlist.hxx"
#include "PlaylistError.hxx"
#include "player/Control.hxx"
#include "song/DetachedSong.hxx"
#include "Listener.hxx"
#include "Log.hxx"

void
playlist::Stop(PlayerControl &pc) noexcept
{
	if (!playing)
		return;

	assert(current >= 0);

	LogDebug(playlist_domain, "stop");
	pc.LockStop();
	queued = -1;
	playing = false;

	if (queue.random) {
		/* shuffle the playlist, so the next playback will
		   result in a new random order */

		unsigned current_position = queue.OrderToPosition(current);

		queue.ShuffleOrder();

		/* make sure that "current" stays valid, and the next
		   "play" command plays the same song again */
		current = queue.PositionToOrder(current_position);
	}
}

unsigned
playlist::MoveOrderToCurrent(unsigned old_order) noexcept
{
	if (!queue.random)
		/* no-op because there is no order list */
		return old_order;

	if (playing) {
		/* already playing: move the specified song after the
		   current one (because the current one has already
		   been playing and shall not be played again) */
		return queue.MoveOrderAfter(old_order, current);
	} else if (current >= 0) {
		/* not playing: move the specified song before the
		   current one, so it will be played eventually */
		return queue.MoveOrderBefore(old_order, current);
	} else {
		/* not playing anything: move the specified song to
		   the front */
		return queue.MoveOrderBefore(old_order, 0);
	}
}

void
playlist::PlayOrder(PlayerControl &pc, unsigned order)
{
	playing = true;
	queued = -1;

	const DetachedSong &song = queue.GetOrder(order);

	FmtDebug(playlist_domain, "play {}:{:?}", order, song.GetURI());

	current = order;

	pc.Play(std::make_unique<DetachedSong>(song));

	SongStarted();
}

void
playlist::PlayAny(PlayerControl &pc)
{
	if (queue.IsEmpty())
		return;

	if (playing) {
		/* already playing: unpause playback, just in
		   case it was paused, and return */
		pc.LockSetPause(false);
		return;
	}

	pc.LockClearError();
	stop_on_error = false;
	error_count = 0;

	/* select a song: "current" song, or the first one */
	unsigned order = current >= 0
		? static_cast<unsigned>(current)
		: 0;

	PlayOrder(pc, order);
}

void
playlist::PlayPosition(PlayerControl &pc, unsigned position)
{
	pc.LockClearError();

	if (!queue.IsValidPosition(position))
		throw PlaylistError::BadRange();

	unsigned order = position;
	if (queue.random) {
		order = queue.PositionToOrder(position);
		order = MoveOrderToCurrent(order);
	}

	stop_on_error = false;
	error_count = 0;

	PlayOrder(pc, order);
}

void
playlist::PlayId(PlayerControl &pc, unsigned id)
{
	int position = queue.IdToPosition(id);
	if (position < 0)
		throw PlaylistError::NoSuchSong();

	PlayPosition(pc, static_cast<unsigned>(position));
}

void
playlist::PlayNext(PlayerControl &pc)
{
	if (!playing)
		throw PlaylistError::NotPlaying();

	assert(!queue.IsEmpty());
	assert(queue.IsValidOrder(current));

	const int old_current = current;
	stop_on_error = false;

	/* determine the next song from the queue's order list */

	const int next_order = queue.GetNextOrder(current);
	if (next_order < 0) {
		/* no song after this one: stop playback */
		Stop(pc);

		/* reset "current song" */
		current = -1;
	}
	else
	{
		if (next_order == 0 && queue.random) {
			/* The queue told us that the next song is the first
			   song.  This means we are in repeat mode.  Shuffle
			   the queue order, so this time, the user hears the
			   songs in a different than before */
			assert(queue.repeat);

			queue.ShuffleOrder();

			/* note that current and queued are
			   now invalid, but PlayOrder() will
			   discard them anyway */
		}

		PlayOrder(pc, next_order);
	}

	/* Consume mode removes each played songs. */
	if (queue.consume != ConsumeMode::OFF)
		DeleteOrder(pc, old_current);

	/* Disable consume mode after consuming one song in oneshot mode. */
	if (queue.consume == ConsumeMode::ONE_SHOT) {
		queue.consume = ConsumeMode::OFF;
		listener.OnQueueOptionsChanged();
	}
}

void
playlist::PlayPrevious(PlayerControl &pc)
{
	if (!playing)
		throw PlaylistError::NotPlaying();

	assert(!queue.IsEmpty());

	int order;
	if (current > 0) {
		/* play the preceding song */
		order = current - 1;
	} else if (queue.repeat) {
		/* play the last song in "repeat" mode */
		order = queue.GetLength() - 1;
	} else {
		/* re-start playing the current song if it's
		   the first one */
		order = current;
	}

	PlayOrder(pc, order);
}

void
playlist::SeekSongOrder(PlayerControl &pc, unsigned i, SongTime seek_time)
{
	assert(queue.IsValidOrder(i));

	pc.LockClearError();
	stop_on_error = true;
	error_count = 0;

	if (!playing || (unsigned)current != i) {
		/* seeking is not within the current song - prepare
		   song change */

		i = MoveOrderToCurrent(i);

		playing = true;
		current = i;
	}

	queued = -1;

	try {
		pc.LockSeek(std::make_unique<DetachedSong>(queue.GetOrder(i)), seek_time);
	} catch (...) {
		UpdateQueuedSong(pc, nullptr);
		throw;
	}

	UpdateQueuedSong(pc, nullptr);
}

void
playlist::SeekSongPosition(PlayerControl &pc, unsigned song,
			   SongTime seek_time)
{
	if (!queue.IsValidPosition(song))
		throw PlaylistError::BadRange();

	unsigned i = queue.random
		? queue.PositionToOrder(song)
		: song;

	SeekSongOrder(pc, i, seek_time);
}

void
playlist::SeekSongId(PlayerControl &pc, unsigned id, SongTime seek_time)
{
	int song = queue.IdToPosition(id);
	if (song < 0)
		throw PlaylistError::NoSuchSong();

	SeekSongPosition(pc, song, seek_time);
}

void
playlist::SeekCurrent(PlayerControl &pc,
		      SignedSongTime seek_time, bool relative)
{
	if (!playing)
		throw PlaylistError::NotPlaying();

	if (relative) {
		const auto status = pc.LockGetStatus();

		if (status.state != PlayerState::PLAY &&
		    status.state != PlayerState::PAUSE)
			throw PlaylistError::NotPlaying();

		seek_time += status.elapsed_time;
	}

	if (seek_time.IsNegative())
		seek_time = SignedSongTime::zero();

	SeekSongOrder(pc, current, SongTime(seek_time));
}
