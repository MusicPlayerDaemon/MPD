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

#include "pcm_resample.h"
#include "config.h"

#include <string.h>

void pcm_resample_init(struct pcm_resample_state *state)
{
	memset(state, 0, sizeof(*state));

#ifdef HAVE_LIBSAMPLERATE
	pcm_buffer_init(&state->in);
	pcm_buffer_init(&state->out);
#endif

	pcm_buffer_init(&state->buffer);
}
