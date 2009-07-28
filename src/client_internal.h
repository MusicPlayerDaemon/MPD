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

#ifndef MPD_CLIENT_INTERNAL_H
#define MPD_CLIENT_INTERNAL_H

#include "client.h"

struct deferred_buffer {
	size_t size;
	char data[sizeof(long)];
};

struct client {
	GIOChannel *channel;
	guint source_id;

	/** the buffer for reading lines from the #channel */
	struct fifo_buffer *input;

	unsigned permission;

	/** the uid of the client process, or -1 if unknown */
	int uid;

	/**
	 * How long since the last activity from this client?
	 */
	GTimer *last_activity;

	GSList *cmd_list;	/* for when in list mode */
	int cmd_list_OK;	/* print OK after each command execution */
	size_t cmd_list_size;	/* mem cmd_list consumes */
	GQueue *deferred_send;	/* for output if client is slow */
	size_t deferred_bytes;	/* mem deferred_send consumes */
	unsigned int num;	/* client number */

	char send_buf[4096];
	size_t send_buf_used;	/* bytes used this instance */

	/** is this client waiting for an "idle" response? */
	bool idle_waiting;

	/** idle flags pending on this client, to be sent as soon as
	    the client enters "idle" */
	unsigned idle_flags;

	/** idle flags that the client wants to receive */
	unsigned idle_subscriptions;
};

#endif
