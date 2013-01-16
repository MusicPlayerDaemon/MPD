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

#include "config.h"
#include "StateFile.hxx"
#include "OutputState.hxx"
#include "PlaylistState.hxx"
#include "TextFile.hxx"
#include "Partition.hxx"
#include "Volume.hxx"
#include "event/Loop.hxx"

#include <glib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "state_file"

StateFile::StateFile(Path &&_path, Partition &_partition, EventLoop &_loop)
	:TimeoutMonitor(_loop), path(std::move(_path)), partition(_partition),
	 prev_volume_version(0), prev_output_version(0),
	 prev_playlist_version(0)
{
	ScheduleSeconds(5 * 60);
}

void
StateFile::Write()
{
	g_debug("Saving state file %s", path.c_str());

	FILE *fp = fopen(path.c_str(), "w");
	if (G_UNLIKELY(!fp)) {
		g_warning("failed to create %s: %s",
			  path.c_str(), g_strerror(errno));
		return;
	}

	save_sw_volume_state(fp);
	audio_output_state_save(fp);
	playlist_state_save(fp, &partition.playlist, &partition.pc);

	fclose(fp);

	prev_volume_version = sw_volume_state_get_hash();
	prev_output_version = audio_output_state_get_version();
	prev_playlist_version = playlist_state_get_hash(&partition.playlist,
							&partition.pc);
}

void
StateFile::Read()
{
	bool success;

	g_debug("Loading state file %s", path.c_str());

	TextFile file(path);
	if (file.HasFailed()) {
		g_warning("failed to open %s: %s",
			  path.c_str(), g_strerror(errno));
		return;
	}

	const char *line;
	while ((line = file.ReadLine()) != NULL) {
		success = read_sw_volume_state(line) ||
			audio_output_state_read(line) ||
			playlist_state_restore(line, file, &partition.playlist,
					       &partition.pc);
		if (!success)
			g_warning("Unrecognized line in state file: %s", line);
	}

	prev_volume_version = sw_volume_state_get_hash();
	prev_output_version = audio_output_state_get_version();
	prev_playlist_version = playlist_state_get_hash(&partition.playlist,
							&partition.pc);
}

inline void
StateFile::AutoWrite()
{
	if (prev_volume_version == sw_volume_state_get_hash() &&
	    prev_output_version == audio_output_state_get_version() &&
	    prev_playlist_version == playlist_state_get_hash(&partition.playlist,
							     &partition.pc))
		/* nothing has changed - don't save the state file,
		   don't spin up the hard disk */
		return;

	Write();
}

/**
 * This function is called every 5 minutes by the GLib main loop, and
 * saves the state file.
 */
bool
StateFile::OnTimeout()
{
	AutoWrite();
	return true;
}
