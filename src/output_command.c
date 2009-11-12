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

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

#include "config.h"
#include "output_command.h"
#include "output_all.h"
#include "output_internal.h"
#include "output_plugin.h"
#include "mixer_control.h"
#include "player_control.h"
#include "idle.h"

extern unsigned audio_output_state_version;

bool
audio_output_enable_index(unsigned idx)
{
	struct audio_output *ao;

	if (idx >= audio_output_count())
		return false;

	ao = audio_output_get(idx);
	if (ao->enabled)
		return true;

	ao->enabled = true;
	idle_add(IDLE_OUTPUT);

	pc_update_audio();

	++audio_output_state_version;

	return true;
}

bool
audio_output_disable_index(unsigned idx)
{
	struct audio_output *ao;
	struct mixer *mixer;

	if (idx >= audio_output_count())
		return false;

	ao = audio_output_get(idx);
	if (!ao->enabled)
		return true;

	ao->enabled = false;
	idle_add(IDLE_OUTPUT);

	mixer = ao->mixer;
	if (mixer != NULL) {
		mixer_close(mixer);
		idle_add(IDLE_MIXER);
	}

	pc_update_audio();

	++audio_output_state_version;

	return true;
}
