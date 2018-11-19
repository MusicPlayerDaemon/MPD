/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "event/MaskMonitor.hxx"
#include "queue/Playlist.hxx"
#include "queue/Listener.hxx"
#include "output/MultipleOutputs.hxx"
#include "mixer/Listener.hxx"
#include "player/Control.hxx"
#include "player/Listener.hxx"
#include "ReplayGainMode.hxx"
#include "SingleMode.hxx"
#include "Chrono.hxx"
#include "util/Compiler.h"
#include "config.h"

#include <string>
#include <memory>

struct Instance;
class MultipleOutputs;
class SongLoader;
class ClientListener;

/**
 * A partition of the Music Player Daemon.  It is a separate unit with
 * a playlist, a player, outputs etc.
 */
struct Partition final : QueueListener, PlayerListener, MixerListener {
	static constexpr unsigned TAG_MODIFIED = 0x1;
	static constexpr unsigned SYNC_WITH_PLAYER = 0x2;
	static constexpr unsigned BORDER_PAUSE = 0x4;

	Instance &instance;

	const std::string name;

	std::unique_ptr<ClientListener> listener;

	MaskMonitor global_events;

	struct playlist playlist;

	MultipleOutputs outputs;

	PlayerControl pc;

	ReplayGainMode replay_gain_mode = ReplayGainMode::OFF;

	Partition(Instance &_instance,
		  const char *_name,
		  unsigned max_length,
		  unsigned buffer_chunks,
		  AudioFormat configured_audio_format,
		  const ReplayGainConfig &replay_gain_config);

	~Partition() noexcept;

	void EmitGlobalEvent(unsigned mask) {
		global_events.OrMask(mask);
	}

	void EmitIdle(unsigned mask);

	void ClearQueue() {
		playlist.Clear(pc);
	}

	unsigned AppendURI(const SongLoader &loader,
			   const char *uri_utf8) {
		return playlist.AppendURI(pc, loader, uri_utf8);
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

	void StaleSong(const char *uri) {
		playlist.StaleSong(pc, uri);
	}

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

	void PlayPosition(int position) {
		return playlist.PlayPosition(pc, position);
	}

	void PlayId(int id) {
		return playlist.PlayId(pc, id);
	}

	void PlayNext() {
		return playlist.PlayNext(pc);
	}

	void PlayPrevious() {
		return playlist.PlayPrevious(pc);
	}

	void SeekSongPosition(unsigned song_position, SongTime seek_time) {
		playlist.SeekSongPosition(pc, song_position, seek_time);
	}

	void SeekSongId(unsigned song_id, SongTime seek_time) {
		playlist.SeekSongId(pc, song_id, seek_time);
	}

	void SeekCurrent(SignedSongTime seek_time, bool relative) {
		playlist.SeekCurrent(pc, seek_time, relative);
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

	void SetSingle(SingleMode new_value) {
		playlist.SetSingle(pc, new_value);
	}

	void SetConsume(bool new_value) {
		playlist.SetConsume(new_value);
	}

	void SetReplayGainMode(ReplayGainMode mode) {
		replay_gain_mode = mode;
		UpdateEffectiveReplayGainMode();
	}

	/**
	 * Publishes the effective #ReplayGainMode to all subsystems.
	 * #ReplayGainMode::AUTO is substituted.
	 */
	void UpdateEffectiveReplayGainMode();

#ifdef ENABLE_DATABASE
	/**
	 * Returns the global #Database instance.  May return nullptr
	 * if this MPD configuration has no database (no
	 * music_directory was configured).
	 */
	const Database *GetDatabase() const;

	const Database &GetDatabaseOrThrow() const;

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
	 * The tag of the given song has been modified.  Propagate the
	 * change to all instances of this song in the queue.
	 */
	void TagModified(const char *uri, const Tag &tag) noexcept;

	/**
	 * Synchronize the player with the play queue.
	 */
	void SyncWithPlayer();

	/**
	 * Border pause has just been enabled. Change single mode to off
	 * if it was one-shot.
	 */
	void BorderPause();

private:
	/* virtual methods from class QueueListener */
	void OnQueueModified() override;
	void OnQueueOptionsChanged() override;
	void OnQueueSongStarted() override;

	/* virtual methods from class PlayerListener */
	void OnPlayerSync() noexcept override;
	void OnPlayerTagModified() noexcept override;
	void OnBorderPause() noexcept override;

	/* virtual methods from class MixerListener */
	void OnMixerVolumeChanged(Mixer &mixer, int volume) override;

	/* callback for #global_events */
	void OnGlobalEvent(unsigned mask);
};

#endif
