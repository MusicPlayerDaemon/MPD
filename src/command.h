/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef COMMAND_H
#define COMMAND_H

#include "gcc.h"
#include "sllist.h"
#include "ack.h"

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
command_process_list(struct client *client,
		     int list_ok, struct strnode *list);

enum command_return
command_process(struct client *client, char *commandString);

void command_success(struct client *client);

mpd_fprintf_ void command_error(struct client *client, enum ack error,
				const char *fmt, ...);

#endif
