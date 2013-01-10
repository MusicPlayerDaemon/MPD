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
#include "Main.hxx"
#include "event/Loop.hxx"

#include <glib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "state_file"

static char *state_file_path;

/** the GLib source id for the save timer */
static guint save_state_source_id;

/**
 * These version numbers determine whether we need to save the state
 * file.  If nothing has changed, we won't let the hard drive spin up.
 */
static unsigned prev_volume_version, prev_output_version,
	prev_playlist_version;

static void
state_file_write(Partition &partition)
{
	FILE *fp;

	assert(state_file_path != NULL);

	g_debug("Saving state file %s", state_file_path);

	fp = fopen(state_file_path, "w");
	if (G_UNLIKELY(!fp)) {
		g_warning("failed to create %s: %s",
			  state_file_path, g_strerror(errno));
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

static void
state_file_read(Partition &partition)
{
	bool success;

	assert(state_file_path != NULL);

	g_debug("Loading state file %s", state_file_path);

	TextFile file(state_file_path);
	if (file.HasFailed()) {
		g_warning("failed to open %s: %s",
			  state_file_path, g_strerror(errno));
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

/**
 * This function is called every 5 minutes by the GLib main loop, and
 * saves the state file.
 */
static gboolean
timer_save_state_file(gpointer data)
{
	Partition &partition = *(Partition *)data;

	if (prev_volume_version == sw_volume_state_get_hash() &&
	    prev_output_version == audio_output_state_get_version() &&
	    prev_playlist_version == playlist_state_get_hash(&partition.playlist,
							     &partition.pc))
		/* nothing has changed - don't save the state file,
		   don't spin up the hard disk */
		return true;

	state_file_write(partition);
	return true;
}

void
state_file_init(const char *path, Partition &partition)
{
	assert(state_file_path == NULL);

	if (path == NULL)
		return;

	state_file_path = g_strdup(path);
	state_file_read(partition);

	save_state_source_id =
		main_loop->AddTimeoutSeconds(5 * 60, timer_save_state_file,
					     &partition);
}

void
state_file_finish(Partition &partition)
{
	if (state_file_path == NULL)
		/* no state file configured, no cleanup required */
		return;

	if (save_state_source_id != 0)
		g_source_remove(save_state_source_id);

	state_file_write(partition);

	g_free(state_file_path);
}
