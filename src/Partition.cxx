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

#include "config.h"
#include "Partition.hxx"
#include "DetachedSong.hxx"
#include "output/MultipleOutputs.hxx"
#include "mixer/Volume.hxx"
#include "Idle.hxx"
#include "GlobalEvents.hxx"

#ifdef ENABLE_DATABASE

void
Partition::DatabaseModified(const Database &db)
{
	playlist.DatabaseModified(db);
}

#endif

void
Partition::TagModified()
{
	DetachedSong *song = pc.LockReadTaggedSong();
	if (song != nullptr) {
		playlist.TagModified(std::move(*song));
		delete song;
	}
}

void
Partition::SyncWithPlayer()
{
	playlist.SyncWithPlayer(pc);
}

void
Partition::OnPlayerSync()
{
	GlobalEvents::Emit(GlobalEvents::PLAYLIST);
}

void
Partition::OnPlayerTagModified()
{
	GlobalEvents::Emit(GlobalEvents::TAG);
}

void
Partition::OnMixerVolumeChanged(gcc_unused Mixer &mixer, gcc_unused int volume)
{
	InvalidateHardwareVolume();

	/* notify clients */
	idle_add(IDLE_MIXER);
}
