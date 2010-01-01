/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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

#ifndef MPD_CLIENT_H
#define MPD_CLIENT_H

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

struct client;
struct sockaddr;

void client_manager_init(void);
void client_manager_deinit(void);

void client_new(int fd, const struct sockaddr *sa, size_t sa_length, int uid);

bool client_is_expired(const struct client *client);

/**
 * returns the uid of the client process, or a negative value if the
 * uid is unknown
 */
int client_get_uid(const struct client *client);

unsigned client_get_permission(const struct client *client);

void client_set_permission(struct client *client, unsigned permission);

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
G_GNUC_PRINTF(2, 3) void client_printf(struct client *client, const char *fmt, ...);

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
bool client_idle_wait(struct client *client, unsigned flags);

#endif
