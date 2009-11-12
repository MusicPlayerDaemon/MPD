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

#include "config.h"
#include "directory_print.h"
#include "directory.h"
#include "client.h"
#include "song_print.h"

static int
dirvec_print(struct client *client, const struct dirvec *dv)
{
	size_t i;

	for (i = 0; i < dv->nr; ++i)
		client_printf(client, DIRECTORY_DIR "%s\n",
			      directory_get_path(dv->base[i]));

	return 0;
}

int
directory_print(struct client *client, const struct directory *directory)
{
	dirvec_print(client, &directory->children);
	songvec_print(client, &directory->songs);

	return 0;
}
