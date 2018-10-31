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

#include "config.h"
#include "Partition.hxx"
#include "Instance.hxx"
#include "song/DetachedSong.hxx"
#include "mixer/Volume.hxx"
#include "IdleFlags.hxx"
#include "client/Listener.hxx"

Partition::Partition(Instance &_instance,
		     const char *_name,
		     unsigned max_length,
		     unsigned buffer_chunks,
		     AudioFormat configured_audio_format,
		     const ReplayGainConfig &replay_gain_config)
	:instance(_instance),
	 name(_name),
	 listener(new ClientListener(instance.event_loop, *this)),
	 global_events(instance.event_loop, BIND_THIS_METHOD(OnGlobalEvent)),
	 playlist(max_length, *this),
	 outputs(*this),
	 pc(*this, outputs, buffer_chunks,
	    configured_audio_format, replay_gain_config)
{
	UpdateEffectiveReplayGainMode();
}

Partition::~Partition() noexcept = default;

void
Partition::EmitIdle(unsigned mask)
{
	instance.EmitIdle(mask);
}

void
Partition::UpdateEffectiveReplayGainMode()
{
	auto mode = replay_gain_mode;
	if (mode == ReplayGainMode::AUTO)
	    mode = playlist.queue.random
		    ? ReplayGainMode::TRACK
		    : ReplayGainMode::ALBUM;

	pc.LockSetReplayGainMode(mode);

	outputs.SetReplayGainMode(mode);
}

#ifdef ENABLE_DATABASE

const Database *
Partition::GetDatabase() const
{
	return instance.GetDatabase();
}

const Database &
Partition::GetDatabaseOrThrow() const
{
	return instance.GetDatabaseOrThrow();
}

void
Partition::DatabaseModified(const Database &db)
{
	playlist.DatabaseModified(db);
	EmitIdle(IDLE_DATABASE);
}

#endif

void
Partition::TagModified()
{
	auto song = pc.LockReadTaggedSong();
	if (song)
		playlist.TagModified(std::move(*song));
}

void
Partition::TagModified(const char *uri, const Tag &tag) noexcept
{
	playlist.TagModified(uri, tag);
}

void
Partition::SyncWithPlayer()
{
	playlist.SyncWithPlayer(pc);
}

void
Partition::BorderPause()
{
	playlist.BorderPause(pc);
}

void
Partition::OnQueueModified()
{
	EmitIdle(IDLE_PLAYLIST);
}

void
Partition::OnQueueOptionsChanged()
{
	EmitIdle(IDLE_OPTIONS);
}

void
Partition::OnQueueSongStarted()
{
	EmitIdle(IDLE_PLAYER);
}

void
Partition::OnPlayerSync() noexcept
{
	EmitGlobalEvent(SYNC_WITH_PLAYER);
}

void
Partition::OnPlayerTagModified() noexcept
{
	EmitGlobalEvent(TAG_MODIFIED);
}

void
Partition::OnBorderPause() noexcept
{
	EmitGlobalEvent(BORDER_PAUSE);
}

void
Partition::OnMixerVolumeChanged(gcc_unused Mixer &mixer, gcc_unused int volume)
{
	InvalidateHardwareVolume();

	/* notify clients */
	EmitIdle(IDLE_MIXER);
}

void
Partition::OnGlobalEvent(unsigned mask)
{
	if ((mask & SYNC_WITH_PLAYER) != 0)
		SyncWithPlayer();

	if ((mask & TAG_MODIFIED) != 0)
		TagModified();

	if ((mask & BORDER_PAUSE) != 0)
		BorderPause();
}
