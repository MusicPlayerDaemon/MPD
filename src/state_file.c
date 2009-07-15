/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "state_file.h"
#include "output_state.h"
#include "playlist.h"
#include "playlist_state.h"
#include "volume.h"

#include <glib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "state_file"

static char *state_file_path;

/** the GLib source id for the save timer */
static guint save_state_source_id;

static void
state_file_write(void)
{
	FILE *fp;

	if (state_file_path == NULL)
		return;

	g_debug("Saving state file %s", state_file_path);

	fp = fopen(state_file_path, "w");
	if (G_UNLIKELY(!fp)) {
		g_warning("failed to create %s: %s",
			  state_file_path, strerror(errno));
		return;
	}

	save_sw_volume_state(fp);
	saveAudioDevicesState(fp);
	playlist_state_save(fp, &g_playlist);

	while(fclose(fp) && errno == EINTR) /* nothing */;
}

static void
state_file_read(void)
{
	FILE *fp;

	assert(state_file_path != NULL);

	g_debug("Loading state file %s", state_file_path);

	fp = fopen(state_file_path, "r");
	if (G_UNLIKELY(!fp)) {
		g_warning("failed to open %s: %s",
			  state_file_path, strerror(errno));
		return;
	}

	read_sw_volume_state(fp);
	rewind(fp);
	readAudioDevicesState(fp);
	rewind(fp);
	playlist_state_restore(fp, &g_playlist);

	while(fclose(fp) && errno == EINTR) /* nothing */;
}

/**
 * This function is called every 5 minutes by the GLib main loop, and
 * saves the state file.
 */
static gboolean
timer_save_state_file(G_GNUC_UNUSED gpointer data)
{
	state_file_write();
	return true;
}

void
state_file_init(const char *path)
{
	assert(state_file_path == NULL);

	if (path == NULL)
		return;

	state_file_path = g_strdup(path);
	state_file_read();

	save_state_source_id = g_timeout_add(5 * 60 * 1000,
					     timer_save_state_file, NULL);
}

void
state_file_finish(void)
{
	if (save_state_source_id != 0)
		g_source_remove(save_state_source_id);

	if (state_file_path != NULL)
		state_file_write();

	g_free(state_file_path);
}
