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

#ifndef MPD_CLIENT_INTERNAL_H
#define MPD_CLIENT_INTERNAL_H

#include "client.h"
#include "client_message.h"
#include "command.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "client"

enum {
	CLIENT_MAX_SUBSCRIPTIONS = 16,
	CLIENT_MAX_MESSAGES = 64,
};

struct deferred_buffer {
	size_t size;
	char data[sizeof(long)];
};

struct client {
	struct player_control *player_control;

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

	char send_buf[16384];
	size_t send_buf_used;	/* bytes used this instance */

	/** is this client waiting for an "idle" response? */
	bool idle_waiting;

	/** idle flags pending on this client, to be sent as soon as
	    the client enters "idle" */
	unsigned idle_flags;

	/** idle flags that the client wants to receive */
	unsigned idle_subscriptions;

	/**
	 * A list of channel names this client is subscribed to.
	 */
	GSList *subscriptions;

	/**
	 * The number of subscriptions in #subscriptions.  Used to
	 * limit the number of subscriptions.
	 */
	unsigned num_subscriptions;

	/**
	 * A list of messages this client has received in reverse
	 * order (latest first).
	 */
	GSList *messages;

	/**
	 * The number of messages in #messages.
	 */
	unsigned num_messages;
};

extern unsigned int client_max_connections;
extern int client_timeout;
extern size_t client_max_command_list_size;
extern size_t client_max_output_buffer_size;

bool
client_list_is_empty(void);

bool
client_list_is_full(void);

struct client *
client_list_get_first(void);

void
client_list_add(struct client *client);

void
client_list_foreach(GFunc func, gpointer user_data);

void
client_list_remove(struct client *client);

void
client_close(struct client *client);

static inline void
new_cmd_list_ptr(struct client *client, const char *s)
{
	client->cmd_list = g_slist_prepend(client->cmd_list, g_strdup(s));
}

static inline void
free_cmd_list(GSList *list)
{
	for (GSList *tmp = list; tmp != NULL; tmp = g_slist_next(tmp))
		g_free(tmp->data);

	g_slist_free(list);
}

void
client_set_expired(struct client *client);

/**
 * Schedule an "expired" check for all clients: permanently delete
 * clients which have been set "expired" with client_set_expired().
 */
void
client_schedule_expire(void);

/**
 * Removes a scheduled "expired" check.
 */
void
client_deinit_expire(void);

enum command_return
client_read(struct client *client);

enum command_return
client_process_line(struct client *client, char *line);

void
client_write_deferred(struct client *client);

void
client_write_output(struct client *client);

gboolean
client_in_event(GIOChannel *source, GIOCondition condition,
		gpointer data);

#endif
