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

#include "client.h"
#include "fifo_buffer.h"
#include "command.h"
#include "conf.h"
#include "listen.h"
#include "socket_util.h"
#include "permission.h"
#include "event_pipe.h"
#include "idle.h"
#include "main.h"
#include "config.h"

#include <glib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "client"
#define LOG_LEVEL_SECURE G_LOG_LEVEL_INFO

static const char GREETING[] = "OK MPD " PROTOCOL_VERSION "\n";

#define CLIENT_LIST_MODE_BEGIN			"command_list_begin"
#define CLIENT_LIST_OK_MODE_BEGIN			"command_list_ok_begin"
#define CLIENT_LIST_MODE_END				"command_list_end"
#define CLIENT_TIMEOUT_DEFAULT			(60)
#define CLIENT_MAX_CONNECTIONS_DEFAULT		(10)
#define CLIENT_MAX_COMMAND_LIST_DEFAULT		(2048*1024)
#define CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT	(8192*1024)

/* set this to zero to indicate we have no possible clients */
static unsigned int client_max_connections;	/*CLIENT_MAX_CONNECTIONS_DEFAULT; */
static int client_timeout;
static size_t client_max_command_list_size;
static size_t client_max_output_buffer_size;

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

	time_t lastTime;
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

static GList *clients;
static unsigned num_clients;
static guint expire_source_id;

static void client_write_deferred(struct client *client);

static void client_write_output(struct client *client);

static void client_manager_expire(void);

static gboolean
client_in_event(GIOChannel *source, GIOCondition condition, gpointer data);

bool client_is_expired(const struct client *client)
{
	return client->channel == NULL;
}

int client_get_uid(const struct client *client)
{
	return client->uid;
}

unsigned client_get_permission(const struct client *client)
{
	return client->permission;
}

void client_set_permission(struct client *client, unsigned permission)
{
	client->permission = permission;
}

/**
 * An idle event which calls client_manager_expire().
 */
static gboolean
client_manager_expire_event(G_GNUC_UNUSED gpointer data)
{
	expire_source_id = 0;
	client_manager_expire();
	return false;
}

static inline void client_set_expired(struct client *client)
{
	if (expire_source_id == 0 && !client_is_expired(client))
		/* delayed deletion */
		expire_source_id = g_idle_add(client_manager_expire_event,
					      NULL);

	if (client->source_id != 0) {
		g_source_remove(client->source_id);
		client->source_id = 0;
	}

	if (client->channel != NULL) {
		g_io_channel_unref(client->channel);
		client->channel = NULL;
	}
}

static void client_init(struct client *client, int fd)
{
	static unsigned int next_client_num;

	assert(fd >= 0);

	client->cmd_list_size = 0;
	client->cmd_list_OK = -1;

#ifndef G_OS_WIN32
	client->channel = g_io_channel_unix_new(fd);
#else
	client->channel = g_io_channel_win32_new_socket(fd);
#endif
	/* GLib is responsible for closing the file descriptor */
	g_io_channel_set_close_on_unref(client->channel, true);
	/* NULL encoding means the stream is binary safe; the MPD
	   protocol is UTF-8 only, but we are doing this call anyway
	   to prevent GLib from messing around with the stream */
	g_io_channel_set_encoding(client->channel, NULL, NULL);
	/* we prefer to do buffering */
	g_io_channel_set_buffered(client->channel, false);

	client->source_id = g_io_add_watch(client->channel,
					   G_IO_IN|G_IO_ERR|G_IO_HUP,
					   client_in_event, client);

	client->input = fifo_buffer_new(4096);

	client->lastTime = time(NULL);
	client->cmd_list = NULL;
	client->deferred_send = g_queue_new();
	client->deferred_bytes = 0;
	client->num = next_client_num++;
	client->send_buf_used = 0;

	client->permission = getDefaultPermissions();

	(void)write(fd, GREETING, sizeof(GREETING) - 1);
}

static void free_cmd_list(GSList *list)
{
	for (GSList *tmp = list; tmp != NULL; tmp = g_slist_next(tmp))
		g_free(tmp->data);

	g_slist_free(list);
}

static void new_cmd_list_ptr(struct client *client, char *s)
{
	client->cmd_list = g_slist_prepend(client->cmd_list, g_strdup(s));
}

static void
deferred_buffer_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct deferred_buffer *buffer = data;
	g_free(buffer);
}

static void client_close(struct client *client)
{
	assert(num_clients > 0);
	assert(clients != NULL);

	clients = g_list_remove(clients, client);
	--num_clients;

	client_set_expired(client);

	if (client->cmd_list) {
		free_cmd_list(client->cmd_list);
		client->cmd_list = NULL;
	}

	g_queue_foreach(client->deferred_send, deferred_buffer_free, NULL);
	g_queue_free(client->deferred_send);

	g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE,
	      "[%u] closed", client->num);
	g_free(client);
}

void client_new(int fd, const struct sockaddr *sa, size_t sa_length, int uid)
{
	struct client *client;
	char *remote;

	if (num_clients >= client_max_connections) {
		g_warning("Max Connections Reached!");
		close(fd);
		return;
	}

	client = g_new0(struct client, 1);
	clients = g_list_prepend(clients, client);
	++num_clients;

	client_init(client, fd);
	client->uid = uid;

	remote = sockaddr_to_string(sa, sa_length, NULL);
	g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE,
	      "[%u] opened from %s", client->num, remote);
	g_free(remote);
}

static int client_process_line(struct client *client, char *line)
{
	int ret = 1;

	if (strcmp(line, "noidle") == 0) {
		if (client->idle_waiting) {
			/* send empty idle response and leave idle mode */
			client->idle_waiting = false;
			command_success(client);
			client_write_output(client);
		}

		/* do nothing if the client wasn't idling: the client
		   has already received the full idle response from
		   client_idle_notify(), which he can now evaluate */

		return 0;
	} else if (client->idle_waiting) {
		/* during idle mode, clients must not send anything
		   except "noidle" */
		g_warning("[%u] command \"%s\" during idle",
			  client->num, line);
		return COMMAND_RETURN_CLOSE;
	}

	if (client->cmd_list_OK >= 0) {
		if (strcmp(line, CLIENT_LIST_MODE_END) == 0) {
			g_debug("[%u] process command list",
				client->num);

			/* for scalability reasons, we have prepended
			   each new command; now we have to reverse it
			   to restore the correct order */
			client->cmd_list = g_slist_reverse(client->cmd_list);

			ret = command_process_list(client,
						   client->cmd_list_OK,
						   client->cmd_list);
			g_debug("[%u] process command "
				"list returned %i", client->num, ret);

			if (ret == COMMAND_RETURN_CLOSE ||
			    client_is_expired(client))
				return COMMAND_RETURN_CLOSE;

			if (ret == 0)
				command_success(client);

			client_write_output(client);
			free_cmd_list(client->cmd_list);
			client->cmd_list = NULL;
			client->cmd_list_OK = -1;
		} else {
			size_t len = strlen(line) + 1;
			client->cmd_list_size += len;
			if (client->cmd_list_size >
			    client_max_command_list_size) {
				g_warning("[%u] command list size (%lu) "
					  "is larger than the max (%lu)",
					  client->num,
					  (unsigned long)client->cmd_list_size,
					  (unsigned long)client_max_command_list_size);
				return COMMAND_RETURN_CLOSE;
			} else
				new_cmd_list_ptr(client, line);
		}
	} else {
		if (strcmp(line, CLIENT_LIST_MODE_BEGIN) == 0) {
			client->cmd_list_OK = 0;
			ret = 1;
		} else if (strcmp(line, CLIENT_LIST_OK_MODE_BEGIN) == 0) {
			client->cmd_list_OK = 1;
			ret = 1;
		} else {
			g_debug("[%u] process command \"%s\"",
				client->num, line);
			ret = command_process(client, line);
			g_debug("[%u] command returned %i",
				client->num, ret);

			if (ret == COMMAND_RETURN_CLOSE ||
			    client_is_expired(client))
				return COMMAND_RETURN_CLOSE;

			if (ret == 0)
				command_success(client);

			client_write_output(client);
		}
	}

	return ret;
}

static char *
client_read_line(struct client *client)
{
	const char *p, *newline;
	size_t length;
	char *line;

	p = fifo_buffer_read(client->input, &length);
	if (p == NULL)
		return NULL;

	newline = memchr(p, '\n', length);
	if (newline == NULL)
		return NULL;

	line = g_strndup(p, newline - p);
	fifo_buffer_consume(client->input, newline - p + 1);

	return g_strchomp(line);
}

static int client_input_received(struct client *client, size_t bytesRead)
{
	char *line;
	int ret;

	fifo_buffer_append(client->input, bytesRead);

	/* process all lines */

	while ((line = client_read_line(client)) != NULL) {
		ret = client_process_line(client, line);
		g_free(line);

		if (ret == COMMAND_RETURN_KILL ||
		    ret == COMMAND_RETURN_CLOSE)
			return ret;
		if (client_is_expired(client))
			return COMMAND_RETURN_CLOSE;
	}

	return 0;
}

static int client_read(struct client *client)
{
	char *p;
	size_t max_length;
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_read;

	p = fifo_buffer_write(client->input, &max_length);
	if (p == NULL) {
		g_warning("[%u] buffer overflow", client->num);
		return COMMAND_RETURN_CLOSE;
	}

	status = g_io_channel_read_chars(client->channel, p, max_length,
					 &bytes_read, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		return client_input_received(client, bytes_read);

	case G_IO_STATUS_AGAIN:
		/* try again later, after select() */
		return 0;

	case G_IO_STATUS_EOF:
		/* peer disconnected */
		return COMMAND_RETURN_CLOSE;

	case G_IO_STATUS_ERROR:
		/* I/O error */
		g_warning("failed to read from client %d: %s",
			  client->num, error->message);
		g_error_free(error);
		return COMMAND_RETURN_CLOSE;
	}

	/* unreachable */
	return COMMAND_RETURN_CLOSE;
}

static gboolean
client_out_event(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
		 gpointer data);

static gboolean
client_in_event(G_GNUC_UNUSED GIOChannel *source,
		GIOCondition condition,
		gpointer data)
{
	struct client *client = data;
	int ret;

	assert(!client_is_expired(client));

	if (condition != G_IO_IN) {
		client_set_expired(client);
		return false;
	}

	client->lastTime = time(NULL);

	ret = client_read(client);
	switch (ret) {
	case COMMAND_RETURN_KILL:
		client_close(client);
		g_main_loop_quit(main_loop);
		return false;

	case COMMAND_RETURN_CLOSE:
		client_close(client);
		return false;
	}

	if (client_is_expired(client)) {
		client_close(client);
		return false;
	}

	if (!g_queue_is_empty(client->deferred_send)) {
		/* deferred buffers exist: schedule write */
		client->source_id = g_io_add_watch(client->channel,
						   G_IO_OUT|G_IO_ERR|G_IO_HUP,
						   client_out_event, client);
		return false;
	}

	/* read more */
	return true;
}

static gboolean
client_out_event(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
		 gpointer data)
{
	struct client *client = data;

	assert(!client_is_expired(client));

	if (condition != G_IO_OUT) {
		client_set_expired(client);
		return false;
	}

	client_write_deferred(client);

	if (client_is_expired(client)) {
		client_close(client);
		return false;
	}

	client->lastTime = time(NULL);

	if (g_queue_is_empty(client->deferred_send)) {
		/* done sending deferred buffers exist: schedule
		   read */
		client->source_id = g_io_add_watch(client->channel,
						   G_IO_IN|G_IO_ERR|G_IO_HUP,
						   client_in_event, client);
		return false;
	}

	/* write more */
	return true;
}

void client_manager_init(void)
{
	client_timeout = config_get_positive(CONF_CONN_TIMEOUT,
					     CLIENT_TIMEOUT_DEFAULT);
	client_max_connections =
		config_get_positive(CONF_MAX_CONN,
				    CLIENT_MAX_CONNECTIONS_DEFAULT);
	client_max_command_list_size =
		config_get_positive(CONF_MAX_COMMAND_LIST_SIZE,
				    CLIENT_MAX_COMMAND_LIST_DEFAULT / 1024)
		* 1024;

	client_max_output_buffer_size =
		config_get_positive(CONF_MAX_OUTPUT_BUFFER_SIZE,
				    CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT / 1024)
		* 1024;
}

static void client_close_all(void)
{
	while (clients != NULL) {
		struct client *client = clients->data;

		client_close(client);
	}

	assert(num_clients == 0);
}

void client_manager_deinit(void)
{
	client_close_all();

	client_max_connections = 0;

	if (expire_source_id != 0)
		g_source_remove(expire_source_id);
}

static void
client_check_expired_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct client *client = data;

	if (client_is_expired(client)) {
		g_debug("[%u] expired", client->num);
		client_close(client);
	} else if (!client->idle_waiting && /* idle clients
					       never expire */
		   time(NULL) - client->lastTime >
		   client_timeout) {
		g_debug("[%u] timeout", client->num);
		client_close(client);
	}
}

static void
client_manager_expire(void)
{
	g_list_foreach(clients, client_check_expired_callback, NULL);
}

static size_t
client_write_deferred_buffer(struct client *client,
			     const struct deferred_buffer *buffer)
{
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_written;

	status = g_io_channel_write_chars
		(client->channel, buffer->data, buffer->size,
		 &bytes_written, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		return bytes_written;

	case G_IO_STATUS_AGAIN:
		return 0;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		client_set_expired(client);
		return 0;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		client_set_expired(client);
		g_warning("failed to flush buffer for %i: %s",
			  client->num, error->message);
		g_error_free(error);
		return 0;
	}

	/* unreachable */
	return 0;
}

static void client_write_deferred(struct client *client)
{
	size_t ret;

	while (!g_queue_is_empty(client->deferred_send)) {
		struct deferred_buffer *buf =
			g_queue_peek_head(client->deferred_send);

		assert(buf->size > 0);
		assert(buf->size <= client->deferred_bytes);

		ret = client_write_deferred_buffer(client, buf);
		if (ret == 0)
			break;

		if (ret < buf->size) {
			assert(client->deferred_bytes >= (size_t)ret);
			client->deferred_bytes -= ret;
			buf->size -= ret;
			memmove(buf->data, buf->data + ret, buf->size);
			break;
		} else {
			size_t decr = sizeof(*buf) -
				sizeof(buf->data) + buf->size;

			assert(client->deferred_bytes >= decr);
			client->deferred_bytes -= decr;
			g_free(buf);
			g_queue_pop_head(client->deferred_send);
		}

		client->lastTime = time(NULL);
	}

	if (g_queue_is_empty(client->deferred_send)) {
		g_debug("[%u] buffer empty %lu", client->num,
			(unsigned long)client->deferred_bytes);
		assert(client->deferred_bytes == 0);
	}
}

static void client_defer_output(struct client *client,
				const void *data, size_t length)
{
	size_t alloc;
	struct deferred_buffer *buf;

	assert(length > 0);

	alloc = sizeof(*buf) - sizeof(buf->data) + length;
	client->deferred_bytes += alloc;
	if (client->deferred_bytes > client_max_output_buffer_size) {
		g_warning("[%u] output buffer size (%lu) is "
			  "larger than the max (%lu)",
			  client->num,
			  (unsigned long)client->deferred_bytes,
			  (unsigned long)client_max_output_buffer_size);
		/* cause client to close */
		client_set_expired(client);
		return;
	}

	buf = g_malloc(alloc);
	buf->size = length;
	memcpy(buf->data, data, length);

	g_queue_push_tail(client->deferred_send, buf);
}

static void client_write_direct(struct client *client,
				const char *data, size_t length)
{
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_written;

	assert(length > 0);
	assert(g_queue_is_empty(client->deferred_send));

	status = g_io_channel_write_chars(client->channel, data, length,
					  &bytes_written, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
	case G_IO_STATUS_AGAIN:
		break;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		client_set_expired(client);
		return;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		client_set_expired(client);
		g_warning("failed to write to %i: %s",
			  client->num, error->message);
		g_error_free(error);
		return;
	}

	if (bytes_written < length)
		client_defer_output(client, data + bytes_written,
				    length - bytes_written);

	if (!g_queue_is_empty(client->deferred_send))
		g_debug("[%u] buffer created", client->num);
}

static void client_write_output(struct client *client)
{
	if (client_is_expired(client) || !client->send_buf_used)
		return;

	if (!g_queue_is_empty(client->deferred_send)) {
		client_defer_output(client, client->send_buf,
				    client->send_buf_used);

		/* try to flush the deferred buffers now; the current
		   server command may take too long to finish, and
		   meanwhile try to feed output to the client,
		   otherwise it will time out.  One reason why
		   deferring is slow might be that currently each
		   client_write() allocates a new deferred buffer.
		   This should be optimized after MPD 0.14. */
		client_write_deferred(client);
	} else
		client_write_direct(client, client->send_buf,
				    client->send_buf_used);

	client->send_buf_used = 0;
}

/**
 * Write a block of data to the client.
 */
static void client_write(struct client *client, const char *buffer, size_t buflen)
{
	/* if the client is going to be closed, do nothing */
	if (client_is_expired(client))
		return;

	while (buflen > 0 && !client_is_expired(client)) {
		size_t copylen;

		assert(client->send_buf_used < sizeof(client->send_buf));

		copylen = sizeof(client->send_buf) - client->send_buf_used;
		if (copylen > buflen)
			copylen = buflen;

		memcpy(client->send_buf + client->send_buf_used, buffer,
		       copylen);
		buflen -= copylen;
		client->send_buf_used += copylen;
		buffer += copylen;
		if (client->send_buf_used >= sizeof(client->send_buf))
			client_write_output(client);
	}
}

void client_puts(struct client *client, const char *s)
{
	client_write(client, s, strlen(s));
}

void client_vprintf(struct client *client, const char *fmt, va_list args)
{
	va_list tmp;
	int length;
	char *buffer;

	va_copy(tmp, args);
	length = vsnprintf(NULL, 0, fmt, tmp);
	va_end(tmp);

	if (length <= 0)
		/* wtf.. */
		return;

	buffer = g_malloc(length + 1);
	vsnprintf(buffer, length + 1, fmt, args);
	client_write(client, buffer, length);
	g_free(buffer);
}

G_GNUC_PRINTF(2, 3) void client_printf(struct client *client, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	client_vprintf(client, fmt, args);
	va_end(args);
}

/**
 * Send "idle" response to this client.
 */
static void
client_idle_notify(struct client *client)
{
	unsigned flags, i;
	const char *const* idle_names;

	assert(client->idle_waiting);
	assert(client->idle_flags != 0);

	flags = client->idle_flags;
	client->idle_flags = 0;
	client->idle_waiting = false;

	idle_names = idle_get_names();
	for (i = 0; idle_names[i]; ++i) {
		if (flags & (1 << i) & client->idle_subscriptions)
			client_printf(client, "changed: %s\n",
				      idle_names[i]);
	}

	client_puts(client, "OK\n");
	client->lastTime = time(NULL);
}

static void
client_idle_callback(gpointer data, gpointer user_data)
{
	struct client *client = data;
	unsigned flags = GPOINTER_TO_UINT(user_data);

	if (client_is_expired(client))
		return;

	client->idle_flags |= flags;
	if (client->idle_waiting
	    && (client->idle_flags & client->idle_subscriptions)) {
		client_idle_notify(client);
		client_write_output(client);
	}
}

void client_manager_idle_add(unsigned flags)
{
	assert(flags != 0);

	g_list_foreach(clients, client_idle_callback, GUINT_TO_POINTER(flags));
}

bool client_idle_wait(struct client *client, unsigned flags)
{
	assert(!client->idle_waiting);

	client->idle_waiting = true;
	client->idle_subscriptions = flags;

	if (client->idle_flags & client->idle_subscriptions) {
		client_idle_notify(client);
		return true;
	} else
		return false;
}
