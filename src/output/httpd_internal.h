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

/** \file
 *
 * Internal declarations for the "httpd" audio output plugin.
 */

#ifndef MPD_OUTPUT_HTTPD_INTERNAL_H
#define MPD_OUTPUT_HTTPD_INTERNAL_H

#include "timer.h"

#include <glib.h>

#include <sys/socket.h>
#include <stdbool.h>

struct httpd_client;

struct httpd_output {
	/**
	 * True if the audio output is open and accepts client
	 * connections.
	 */
	bool open;

	/**
	 * The configured encoder plugin.
	 */
	struct encoder *encoder;

	/**
	 * The MIME type produced by the #encoder.
	 */
	const char *content_type;

	/**
	 * The configured address of the listener socket.
	 */
	struct sockaddr_storage address;

	/**
	 * The size of #address.
	 */
	socklen_t address_size;

	/**
	 * This mutex protects the listener socket and the client
	 * list.
	 */
	GMutex *mutex;

	/**
	 * A #Timer object to synchronize this output with the
	 * wallclock.
	 */
	Timer *timer;

	/**
	 * The listener socket.
	 */
	int fd;

	/**
	 * A GLib main loop source id for the listener socket.
	 */
	guint source_id;

	/**
	 * The header page, which is sent to every client on connect.
	 */
	struct page *header;

	/**
	 * The metadata, which is sent to every client.
	 */
	struct page *metadata;

	/**
	 * A linked list containing all clients which are currently
	 * connected.
	 */
	GList *clients;

	/**
	 * A temporary buffer for the httpd_output_read_page()
	 * function.
	 */
	char buffer[32768];

	/**
	 * The maximum and current number of clients connected 
	 * at the same time.
	 */
	guint clients_max, clients_cnt;
};

/**
 * Removes a client from the httpd_output.clients linked list.
 */
void
httpd_output_remove_client(struct httpd_output *httpd,
			   struct httpd_client *client);

/**
 * Sends the encoder header to the client.  This is called right after
 * the response headers have been sent.
 */
void
httpd_output_send_header(struct httpd_output *httpd,
			 struct httpd_client *client);

#endif
