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

#ifndef MPD_COMMAND_H
#define MPD_COMMAND_H

#include "ack.h"

#include <glib.h>
#include <stdbool.h>

enum command_return {
	COMMAND_RETURN_ERROR = -1,
	COMMAND_RETURN_OK = 0,
	COMMAND_RETURN_KILL = 10,
	COMMAND_RETURN_CLOSE = 20,
};

struct client;

void command_init(void);

void command_finish(void);

enum command_return
command_process(struct client *client, unsigned num, char *line);

void command_success(struct client *client);

#endif
