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
#include "sticker_print.h"
#include "sticker.h"
#include "client.h"

void
sticker_print_value(struct client *client,
		    const char *name, const char *value)
{
	client_printf(client, "sticker: %s=%s\n", name, value);
}

static void
print_sticker_cb(const char *name, const char *value, gpointer data)
{
	struct client *client = data;

	sticker_print_value(client, name, value);
}

void
sticker_print(struct client *client, const struct sticker *sticker)
{
	sticker_foreach(sticker, print_sticker_cb, client);
}
