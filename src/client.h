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

#ifndef MPD_CLIENT_H
#define MPD_CLIENT_H

#include "gcc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <sys/socket.h>

struct client;

void client_manager_init(void);
void client_manager_deinit(void);
int client_manager_io(void);
void client_manager_expire(void);

void client_new(int fd, const struct sockaddr *addr, int uid);

int client_is_expired(const struct client *client);

/**
 * returns the uid of the client process, or a negative value if the
 * uid is unknown
 */
int client_get_uid(const struct client *client);

unsigned client_get_permission(const struct client *client);

void client_set_permission(struct client *client, unsigned permission);

/**
 * Write a block of data to the client.
 */
void client_write(struct client *client, const char *data, size_t length);

/**
 * Write a C string to the client.
 */
void client_puts(struct client *client, const char *s);

/**
 * Write a printf-like formatted string to the client.
 */
void client_vprintf(struct client *client, const char *fmt, va_list args);

/**
 * Write a printf-like formatted string to the client.
 */
mpd_fprintf void client_printf(struct client *client, const char *fmt, ...);

/**
 * Adds the specified idle flags to all clients and immediately sends
 * notifications to all waiting clients.
 */
void client_manager_idle_add(unsigned flags);

/**
 * Checks whether the client has pending idle flags.  If yes, they are
 * sent immediately and "true" is returned".  If no, it puts the
 * client into waiting mode and returns false.
 */
bool client_idle_wait(struct client *client);

#endif
