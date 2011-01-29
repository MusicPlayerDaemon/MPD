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

#ifndef MPD_CLIENT_IDLE_H
#define MPD_CLIENT_IDLE_H

#include <stdbool.h>

struct client;

void
client_idle_add(struct client *client, unsigned flags);

/**
 * Adds the specified idle flags to all clients and immediately sends
 * notifications to all waiting clients.
 */
void
client_manager_idle_add(unsigned flags);

/**
 * Checks whether the client has pending idle flags.  If yes, they are
 * sent immediately and "true" is returned".  If no, it puts the
 * client into waiting mode and returns false.
 */
bool
client_idle_wait(struct client *client, unsigned flags);

#endif
