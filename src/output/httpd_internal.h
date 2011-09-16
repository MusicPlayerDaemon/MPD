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

/** \file
 *
 * Internal declarations for the "httpd" audio output plugin.
 */

#ifndef MPD_OUTPUT_HTTPD_INTERNAL_H
#define MPD_OUTPUT_HTTPD_INTERNAL_H

#include "output_internal.h"
#include "timer.h"

#include <glib.h>

#include <stdbool.h>

struct httpd_client;

struct httpd_output {
	struct audio_output base;

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
	 * Number of bytes which were fed into the encoder, without
	 * ever receiving new output.  This is used to estimate
	 * whether MPD should manually flush the encoder, to avoid
	 * buffer underruns in the client.
	 */
	size_t unflushed_input;

	/**
	 * The MIME type produced by the #encoder.
	 */
	const char *content_type;

	/**
	 * This mutex protects the listener socket and the client
	 * list.
	 */
	GMutex *mutex;

	/**
	 * A #timer object to synchronize this output with the
	 * wallclock.
	 */
	struct timer *timer;

	/**
	 * The listener socket.
	 */
	struct server_socket *server_socket;

	/**
	 * The header page, which is sent to every client on connect.
	 */
	struct page *header;

	/**
	 * The metadata, which is sent to every client.
	 */
	struct page *metadata;

	/**
	 * The configured name.
	 */
	char const *name;
	/**
	 * The configured genre.
	 */
	char const *genre;
	/**
	 * The configured website address.
	 */
	char const *website;

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
