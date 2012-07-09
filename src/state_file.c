/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "state_file.h"
#include "output_state.h"
#include "playlist.h"
#include "playlist_state.h"
#include "volume.h"
#include "text_file.h"

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
state_file_write(struct player_control *pc)
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
	playlist_state_save(fp, &g_playlist, pc);

	fclose(fp);

	prev_volume_version = sw_volume_state_get_hash();
	prev_output_version = audio_output_state_get_version();
	prev_playlist_version = playlist_state_get_hash(&g_playlist, pc);
}

static void
state_file_read(struct player_control *pc)
{
	FILE *fp;
	bool success;

	assert(state_file_path != NULL);

	g_debug("Loading state file %s", state_file_path);

	fp = fopen(state_file_path, "r");
	if (G_UNLIKELY(!fp)) {
		g_warning("failed to open %s: %s",
			  state_file_path, g_strerror(errno));
		return;
	}

	GString *buffer = g_string_sized_new(1024);
	const char *line;
	while ((line = read_text_line(fp, buffer)) != NULL) {
		success = read_sw_volume_state(line) ||
			audio_output_state_read(line) ||
			playlist_state_restore(line, fp, buffer,
					       &g_playlist, pc);
		if (!success)
			g_warning("Unrecognized line in state file: %s", line);
	}

	fclose(fp);

	prev_volume_version = sw_volume_state_get_hash();
	prev_output_version = audio_output_state_get_version();
	prev_playlist_version = playlist_state_get_hash(&g_playlist, pc);


	g_string_free(buffer, true);
}

/**
 * This function is called every 5 minutes by the GLib main loop, and
 * saves the state file.
 */
static gboolean
timer_save_state_file(gpointer data)
{
	struct player_control *pc = data;

	if (prev_volume_version == sw_volume_state_get_hash() &&
	    prev_output_version == audio_output_state_get_version() &&
	    prev_playlist_version == playlist_state_get_hash(&g_playlist, pc))
		/* nothing has changed - don't save the state file,
		   don't spin up the hard disk */
		return true;

	state_file_write(pc);
	return true;
}

void
state_file_init(const char *path, struct player_control *pc)
{
	assert(state_file_path == NULL);

	if (path == NULL)
		return;

	state_file_path = g_strdup(path);
	state_file_read(pc);

	save_state_source_id = g_timeout_add_seconds(5 * 60,
						     timer_save_state_file,
						     pc);
}

void
state_file_finish(struct player_control *pc)
{
	if (state_file_path == NULL)
		/* no state file configured, no cleanup required */
		return;

	if (save_state_source_id != 0)
		g_source_remove(save_state_source_id);

	state_file_write(pc);

	g_free(state_file_path);
}
