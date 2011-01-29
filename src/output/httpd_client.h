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

#ifndef MPD_OUTPUT_HTTPD_CLIENT_H
#define MPD_OUTPUT_HTTPD_CLIENT_H

#include <glib.h>

#include <stdbool.h>

struct httpd_client;
struct httpd_output;
struct page;

/**
 * Creates a new #httpd_client object
 *
 * @param httpd the HTTP output device
 * @param fd the socket file descriptor
 */
struct httpd_client *
httpd_client_new(struct httpd_output *httpd, int fd, bool metadata_supported);

/**
 * Frees memory and resources allocated by the #httpd_client object.
 * This does not remove it from the #httpd_output object.
 */
void
httpd_client_free(struct httpd_client *client);

/**
 * Returns the total size of this client's page queue.
 */
size_t
httpd_client_queue_size(const struct httpd_client *client);

/**
 * Clears the page queue.
 */
void
httpd_client_cancel(struct httpd_client *client);

/**
 * Appends a page to the client's queue.
 */
void
httpd_client_send(struct httpd_client *client, struct page *page);

/**
 * Sends the passed metadata.
 */
void
httpd_client_send_metadata(struct httpd_client *client, struct page *page);

#endif
