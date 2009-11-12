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
 * Protocol specific code for the audio output library.
 *
 */

#include "config.h"
#include "output_print.h"
#include "output_internal.h"
#include "output_all.h"
#include "client.h"

void
printAudioDevices(struct client *client)
{
	unsigned n = audio_output_count();

	for (unsigned i = 0; i < n; ++i) {
		const struct audio_output *ao = audio_output_get(i);

		client_printf(client,
			      "outputid: %i\n"
			      "outputname: %s\n"
			      "outputenabled: %i\n",
			      i, ao->name, ao->enabled);
	}
}
