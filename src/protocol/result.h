/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#ifndef MPD_PROTOCOL_RESULT_H
#define MPD_PROTOCOL_RESULT_H

#include "check.h"
#include "ack.h"

#include <glib.h>

struct client;

extern const char *current_command;
extern int command_list_num;

void
command_success(struct client *client);

void
command_error_v(struct client *client, enum ack error,
		const char *fmt, va_list args);

G_GNUC_PRINTF(3, 4)
void
command_error(struct client *client, enum ack error, const char *fmt, ...);

#endif
