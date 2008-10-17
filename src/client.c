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

#include "client.h"
#include "command.h"
#include "conf.h"
#include "log.h"
#include "listen.h"
#include "permission.h"
#include "sllist.h"
#include "utils.h"
#include "ioops.h"
#include "main_notify.h"
#include "dlist.h"
#include "idle.h"

#include "../config.h"

#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define GREETING				"OK MPD " PROTOCOL_VERSION "\n"

#define CLIENT_MAX_BUFFER_LENGTH			(40960)
#define CLIENT_LIST_MODE_BEGIN			"command_list_begin"
#define CLIENT_LIST_OK_MODE_BEGIN			"command_list_ok_begin"
#define CLIENT_LIST_MODE_END				"command_list_end"
#define CLIENT_DEFAULT_OUT_BUFFER_SIZE		(4096)
#define CLIENT_TIMEOUT_DEFAULT			(60)
#define CLIENT_MAX_CONNECTIONS_DEFAULT		(10)
#define CLIENT_MAX_COMMAND_LIST_DEFAULT		(2048*1024)
#define CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT	(8192*1024)

/* set this to zero to indicate we have no possible clients */
static unsigned int client_max_connections;	/*CLIENT_MAX_CONNECTIONS_DEFAULT; */
static int client_timeout = CLIENT_TIMEOUT_DEFAULT;
static size_t client_max_command_list_size =
    CLIENT_MAX_COMMAND_LIST_DEFAULT;
static size_t client_max_output_buffer_size =
    CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT;

/* maybe make conf option for this, or... 32 might be good enough */
static long int client_list_cache_size = 32;

/* shared globally between all clients: */
static struct strnode *list_cache;
static struct strnode *list_cache_head;
static struct strnode *list_cache_tail;

struct client {
	struct list_head siblings;

	char buffer[CLIENT_MAX_BUFFER_LENGTH];
	size_t bufferLength;
	size_t bufferPos;

	int fd;	/* file descriptor; -1 if expired */
	int permission;

	/** the uid of the client process, or -1 if unknown */
	int uid;

	time_t lastTime;
	struct strnode *cmd_list;	/* for when in list mode */
	struct strnode *cmd_list_tail;	/* for when in list mode */
	int cmd_list_OK;	/* print OK after each command execution */
	size_t cmd_list_size;	/* mem cmd_list consumes */
	int cmd_list_dup;	/* has the cmd_list been copied to private space? */
	struct sllnode *deferred_send;	/* for output if client is slow */
	size_t deferred_bytes;	/* mem deferred_send consumes */
	unsigned int num;	/* client number */

	char *send_buf;
	size_t send_buf_used;	/* bytes used this instance */
	size_t send_buf_size;	/* bytes usable this instance */
	size_t send_buf_alloc;	/* bytes actually allocated */

	/** is this client waiting for an "idle" response? */
	bool idle_waiting;

	/** idle flags pending on this client, to be sent as soon as
	    the client enters "idle" */
	unsigned idle_flags;
};

static LIST_HEAD(clients);
static unsigned num_clients;

static void client_write_deferred(struct client *client);

static void client_write_output(struct client *client);

#ifdef SO_SNDBUF
static size_t get_default_snd_buf_size(struct client *client)
{
	int new_size;
	socklen_t sockOptLen = sizeof(int);

	if (getsockopt(client->fd, SOL_SOCKET, SO_SNDBUF,
		       (char *)&new_size, &sockOptLen) < 0) {
		DEBUG("problem getting sockets send buffer size\n");
		return CLIENT_DEFAULT_OUT_BUFFER_SIZE;
	}
	if (new_size > 0)
		return (size_t)new_size;
	DEBUG("sockets send buffer size is not positive\n");
	return CLIENT_DEFAULT_OUT_BUFFER_SIZE;
}
#else /* !SO_SNDBUF */
static size_t get_default_snd_buf_size(struct client *client)
{
	return CLIENT_DEFAULT_OUT_BUFFER_SIZE;
}
#endif /* !SO_SNDBUF */

static void set_send_buf_size(struct client *client)
{
	size_t new_size = get_default_snd_buf_size(client);
	if (client->send_buf_size != new_size) {
		client->send_buf_size = new_size;
		/* don't resize to get smaller, only bigger */
		if (client->send_buf_alloc < new_size) {
			if (client->send_buf)
				free(client->send_buf);
			client->send_buf = xmalloc(new_size);
			client->send_buf_alloc = new_size;
		}
	}
}

int client_is_expired(const struct client *client)
{
	return client->fd < 0;
}

int client_get_uid(const struct client *client)
{
	return client->uid;
}

int client_get_permission(const struct client *client)
{
	return client->permission;
}

void client_set_permission(struct client *client, int permission)
{
	client->permission = permission;
}

static inline void client_set_expired(struct client *client)
{
	if (client->fd >= 0) {
		xclose(client->fd);
		client->fd = -1;
	}
}

static void client_init(struct client *client, int fd)
{
	static unsigned int next_client_num;

	assert(fd >= 0);

	client->cmd_list_size = 0;
	client->cmd_list_dup = 0;
	client->cmd_list_OK = -1;
	client->bufferLength = 0;
	client->bufferPos = 0;
	client->fd = fd;
	set_nonblocking(fd);
	client->lastTime = time(NULL);
	client->cmd_list = NULL;
	client->cmd_list_tail = NULL;
	client->deferred_send = NULL;
	client->deferred_bytes = 0;
	client->num = next_client_num++;
	client->send_buf_used = 0;

	client->permission = getDefaultPermissions();
	set_send_buf_size(client);

	xwrite(fd, GREETING, strlen(GREETING));
}

static void free_cmd_list(struct strnode *list)
{
	struct strnode *tmp = list;

	while (tmp) {
		struct strnode *next = tmp->next;
		if (tmp >= list_cache_head && tmp <= list_cache_tail) {
			/* inside list_cache[] array */
			tmp->data = NULL;
			tmp->next = NULL;
		} else
			free(tmp);
		tmp = next;
	}
}

static void cmd_list_clone(struct client *client)
{
	struct strnode *new = dup_strlist(client->cmd_list);
	free_cmd_list(client->cmd_list);
	client->cmd_list = new;
	client->cmd_list_dup = 1;

	/* new tail */
	while (new && new->next)
		new = new->next;
	client->cmd_list_tail = new;
}

static void new_cmd_list_ptr(struct client *client, char *s, const int size)
{
	int i;
	struct strnode *new;

	if (!client->cmd_list_dup) {
		for (i = client_list_cache_size - 1; i >= 0; --i) {
			if (list_cache[i].data)
				continue;
			new = &(list_cache[i]);
			new->data = s;
			/* implied in free_cmd_list() and init: */
			/* last->next->next = NULL; */
			goto out;
		}
	}

	/* allocate from the heap */
	new = client->cmd_list_dup ? new_strnode_dup(s, size)
	                              : new_strnode(s);
out:
	if (client->cmd_list) {
		client->cmd_list_tail->next = new;
		client->cmd_list_tail = new;
	} else
		client->cmd_list = client->cmd_list_tail = new;
}

static void client_close(struct client *client)
{
	struct sllnode *buf;

	assert(num_clients > 0);
	assert(!list_empty(&clients));
	list_del(&client->siblings);
	--num_clients;

	client_set_expired(client);

	if (client->cmd_list) {
		free_cmd_list(client->cmd_list);
		client->cmd_list = NULL;
	}

	if ((buf = client->deferred_send)) {
		do {
			struct sllnode *prev = buf;
			buf = buf->next;
			free(prev);
		} while (buf);
		client->deferred_send = NULL;
	}

	if (client->send_buf)
		free(client->send_buf);

	SECURE("client %i: closed\n", client->num);
	free(client);
}

static const char *
sockaddr_to_tmp_string(const struct sockaddr *addr)
{
	const char *hostname;

	switch (addr->sa_family) {
#ifdef HAVE_TCP
	case AF_INET:
		hostname = (const char *)inet_ntoa(((const struct sockaddr_in *)
						    addr)->sin_addr);
		if (!hostname)
			hostname = "error getting ipv4 address";
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		{
			static char host[INET6_ADDRSTRLEN + 1];
			memset(host, 0, INET6_ADDRSTRLEN + 1);
			if (inet_ntop(AF_INET6, (const void *)
				      &(((const struct sockaddr_in6 *)addr)->
					sin6_addr), host,
				      INET6_ADDRSTRLEN)) {
				hostname = (const char *)host;
			} else {
				hostname = "error getting ipv6 address";
			}
		}
		break;
#endif
#endif /* HAVE_TCP */
#ifdef HAVE_UN
	case AF_UNIX:
		hostname = "local connection";
		break;
#endif /* HAVE_UN */
	default:
		hostname = "unknown";
	}

	return hostname;
}

void client_new(int fd, const struct sockaddr *addr, int uid)
{
	struct client *client;

	if (num_clients >= client_max_connections) {
		ERROR("Max Connections Reached!\n");
		xclose(fd);
		return;
	}

	client = xcalloc(1, sizeof(*client));
	list_add(&client->siblings, &clients);
	++num_clients;
	client_init(client, fd);
	client->uid = uid;
	SECURE("client %i: opened from %s\n", client->num,
	       sockaddr_to_tmp_string(addr));
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
		ERROR("client %i: command \"%s\" during idle\n",
		      client->num, line);
		return COMMAND_RETURN_CLOSE;
	}

	if (client->cmd_list_OK >= 0) {
		if (strcmp(line, CLIENT_LIST_MODE_END) == 0) {
			DEBUG("client %i: process command "
			      "list\n", client->num);
			ret = processListOfCommands(client,
						    client->cmd_list_OK,
						    client->cmd_list);
			DEBUG("client %i: process command "
			      "list returned %i\n", client->num, ret);

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
				ERROR("client %i: command "
				      "list size (%lu) is "
				      "larger than the max "
				      "(%lu)\n",
				      client->num,
				      (unsigned long)client->cmd_list_size,
				      (unsigned long)
				      client_max_command_list_size);
				return COMMAND_RETURN_CLOSE;
			} else
				new_cmd_list_ptr(client, line, len);
		}
	} else {
		if (strcmp(line, CLIENT_LIST_MODE_BEGIN) == 0) {
			client->cmd_list_OK = 0;
			ret = 1;
		} else if (strcmp(line, CLIENT_LIST_OK_MODE_BEGIN) == 0) {
			client->cmd_list_OK = 1;
			ret = 1;
		} else {
			DEBUG("client %i: process command \"%s\"\n",
			      client->num, line);
			ret = processCommand(client, line);
			DEBUG("client %i: command returned %i\n",
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

static int client_input_received(struct client *client, size_t bytesRead)
{
	char *start = client->buffer + client->bufferPos, *end;
	char *newline, *next;
	int ret;

	assert(client->bufferPos <= client->bufferLength);
	assert(client->bufferLength + bytesRead <= sizeof(client->buffer));

	client->bufferLength += bytesRead;
	end = client->buffer + client->bufferLength;

	/* process all lines */
	while ((newline = memchr(start, '\n', end - start)) != NULL) {
		next = newline + 1;

		if (newline > start && newline[-1] == '\r')
			--newline;
		*newline = 0;

		ret = client_process_line(client, start);
		if (ret == COMMAND_RETURN_KILL ||
		    ret == COMMAND_RETURN_CLOSE)
			return ret;
		if (client_is_expired(client))
			return COMMAND_RETURN_CLOSE;

		start = next;
	}

	/* mark consumed lines */
	client->bufferPos = start - client->buffer;

	/* if we're have reached the buffer's end, close the gab at
	   the beginning */
	if (client->bufferLength == sizeof(client->buffer)) {
		if (client->bufferPos == 0) {
			ERROR("client %i: buffer overflow\n",
			      client->num);
			return COMMAND_RETURN_CLOSE;
		}
		if (client->cmd_list_OK >= 0 &&
		    client->cmd_list &&
		    !client->cmd_list_dup)
			cmd_list_clone(client);
		assert(client->bufferLength >= client->bufferPos
		       && "bufferLength >= bufferPos");
		client->bufferLength -= client->bufferPos;
		memmove(client->buffer,
			client->buffer + client->bufferPos,
			client->bufferLength);
		client->bufferPos = 0;
	}

	return 0;
}

static int client_read(struct client *client)
{
	ssize_t bytesRead;

	assert(client->bufferPos <= client->bufferLength);
	assert(client->bufferLength < sizeof(client->buffer));

	bytesRead = read(client->fd,
			 client->buffer + client->bufferLength,
			 CLIENT_MAX_BUFFER_LENGTH - client->bufferLength);

	if (bytesRead > 0)
		return client_input_received(client, bytesRead);
	else if (bytesRead < 0 && errno == EINTR)
		/* try again later, after select() */
		return 0;
	else
		/* peer disconnected or I/O error */
		return COMMAND_RETURN_CLOSE;
}

static void client_manager_register_read_fd(fd_set * fds, int *fdmax)
{
	struct client *client;

	FD_ZERO(fds);
	addListenSocketsToFdSet(fds, fdmax);

	list_for_each_entry(client, &clients, siblings) {
		if (!client_is_expired(client) && !client->deferred_send) {
			FD_SET(client->fd, fds);
			if (*fdmax < client->fd)
				*fdmax = client->fd;
		}
	}
}

static void client_manager_register_write_fd(fd_set * fds, int *fdmax)
{
	struct client *client;

	FD_ZERO(fds);

	list_for_each_entry(client, &clients, siblings) {
		if (client->fd >= 0 && !client_is_expired(client)
		    && client->deferred_send) {
			FD_SET(client->fd, fds);
			if (*fdmax < client->fd)
				*fdmax = client->fd;
		}
	}
}

int client_manager_io(void)
{
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	struct client *client, *n;
	int ret;
	int fdmax = 0;

	FD_ZERO( &efds );
	client_manager_register_read_fd(&rfds, &fdmax);
	client_manager_register_write_fd(&wfds, &fdmax);

	registered_IO_add_fds(&fdmax, &rfds, &wfds, &efds);

	main_notify_lock();
	ret = select(fdmax + 1, &rfds, &wfds, &efds, NULL);
	main_notify_unlock();

	if (ret < 0) {
		if (errno == EINTR)
			return 0;

		FATAL("select() failed: %s\n", strerror(errno));
	}

	registered_IO_consume_fds(&ret, &rfds, &wfds, &efds);

	getConnections(&rfds);

	list_for_each_entry_safe(client, n, &clients, siblings) {
		if (FD_ISSET(client->fd, &rfds)) {
			ret = client_read(client);
			if (ret == COMMAND_RETURN_KILL)
				return COMMAND_RETURN_KILL;
			if (ret == COMMAND_RETURN_CLOSE) {
				client_close(client);
				continue;
			}

			assert(!client_is_expired(client));

			client->lastTime = time(NULL);
		}
		if (!client_is_expired(client) &&
		    FD_ISSET(client->fd, &wfds)) {
			client_write_deferred(client);
			client->lastTime = time(NULL);
		}
	}

	return 0;
}

void client_manager_init(void)
{
	char *test;
	ConfigParam *param;

	param = getConfigParam(CONF_CONN_TIMEOUT);

	if (param) {
		client_timeout = strtol(param->value, &test, 10);
		if (*test != '\0' || client_timeout <= 0) {
			FATAL("connection timeout \"%s\" is not a positive "
			      "integer, line %i\n", CONF_CONN_TIMEOUT,
			      param->line);
		}
	}

	param = getConfigParam(CONF_MAX_CONN);

	if (param) {
		client_max_connections = strtol(param->value, &test, 10);
		if (*test != '\0' || client_max_connections <= 0) {
			FATAL("max connections \"%s\" is not a positive integer"
			      ", line %i\n", param->value, param->line);
		}
	} else
		client_max_connections = CLIENT_MAX_CONNECTIONS_DEFAULT;

	param = getConfigParam(CONF_MAX_COMMAND_LIST_SIZE);

	if (param) {
		long tmp = strtol(param->value, &test, 10);
		if (*test != '\0' || tmp <= 0) {
			FATAL("max command list size \"%s\" is not a positive "
			      "integer, line %i\n", param->value, param->line);
		}
		client_max_command_list_size = tmp * 1024;
	}

	param = getConfigParam(CONF_MAX_OUTPUT_BUFFER_SIZE);

	if (param) {
		long tmp = strtol(param->value, &test, 10);
		if (*test != '\0' || tmp <= 0) {
			FATAL("max output buffer size \"%s\" is not a positive "
			      "integer, line %i\n", param->value, param->line);
		}
		client_max_output_buffer_size = tmp * 1024;
	}

	list_cache = xcalloc(client_list_cache_size, sizeof(struct strnode));
	list_cache_head = &(list_cache[0]);
	list_cache_tail = &(list_cache[client_list_cache_size - 1]);
}

static void client_close_all(void)
{
	struct client *client, *n;

	list_for_each_entry_safe(client, n, &clients, siblings)
		client_close(client);
	num_clients = 0;

	free(list_cache);
}

void client_manager_deinit(void)
{
	client_close_all();

	client_max_connections = 0;
}

void client_manager_expire(void)
{
	struct client *client, *n;

	list_for_each_entry_safe(client, n, &clients, siblings) {
		if (client_is_expired(client)) {
			DEBUG("client %i: expired\n", client->num);
			client_close(client);
		} else if (!client->idle_waiting && /* idle clients
						       never expire */
			   time(NULL) - client->lastTime >
			   client_timeout) {
			DEBUG("client %i: timeout\n", client->num);
			client_close(client);
		}
	}
}

static void client_write_deferred(struct client *client)
{
	struct sllnode *buf;
	ssize_t ret = 0;

	buf = client->deferred_send;
	while (buf) {
		assert(buf->size > 0);

		ret = write(client->fd, buf->data, buf->size);
		if (ret < 0)
			break;
		else if ((size_t)ret < buf->size) {
			assert(client->deferred_bytes >= (size_t)ret);
			client->deferred_bytes -= ret;
			buf->data = (char *)buf->data + ret;
			buf->size -= ret;
		} else {
			struct sllnode *tmp = buf;
			size_t decr = (buf->size + sizeof(struct sllnode));

			assert(client->deferred_bytes >= decr);
			client->deferred_bytes -= decr;
			buf = buf->next;
			free(tmp);
			client->deferred_send = buf;
		}
		client->lastTime = time(NULL);
	}

	if (!client->deferred_send) {
		DEBUG("client %i: buffer empty %lu\n", client->num,
		      (unsigned long)client->deferred_bytes);
		assert(client->deferred_bytes == 0);
	} else if (ret < 0 && errno != EAGAIN && errno != EINTR) {
		/* cause client to close */
		DEBUG("client %i: problems flushing buffer\n",
		      client->num);
		client_set_expired(client);
	}
}

static void client_defer_output(struct client *client,
				const void *data, size_t length)
{
	struct sllnode **buf_r;

	assert(length > 0);

	client->deferred_bytes += sizeof(struct sllnode) + length;
	if (client->deferred_bytes > client_max_output_buffer_size) {
		ERROR("client %i: output buffer size (%lu) is "
		      "larger than the max (%lu)\n",
		      client->num,
		      (unsigned long)client->deferred_bytes,
		      (unsigned long)client_max_output_buffer_size);
		/* cause client to close */
		client_set_expired(client);
		return;
	}

	buf_r = &client->deferred_send;
	while (*buf_r != NULL)
		buf_r = &(*buf_r)->next;
	*buf_r = new_sllnode(data, length);
}

static void client_write_direct(struct client *client,
				const char *data, size_t length)
{
	ssize_t ret;

	assert(length > 0);
	assert(client->deferred_send == NULL);

	if ((ret = write(client->fd, data, length)) < 0) {
		if (errno == EAGAIN || errno == EINTR) {
			client_defer_output(client, data, length);
		} else {
			DEBUG("client %i: problems writing\n", client->num);
			client_set_expired(client);
			return;
		}
	} else if ((size_t)ret < client->send_buf_used) {
		client_defer_output(client, data + ret, length - ret);
	}

	if (client->deferred_send)
		DEBUG("client %i: buffer created\n", client->num);
}

static void client_write_output(struct client *client)
{
	if (client_is_expired(client) || !client->send_buf_used)
		return;

	if (client->deferred_send != NULL)
		client_defer_output(client, client->send_buf,
				    client->send_buf_used);
	else
		client_write_direct(client, client->send_buf,
				    client->send_buf_used);

	client->send_buf_used = 0;
}

void client_write(struct client *client, const char *buffer, size_t buflen)
{
	size_t copylen;

	/* if the client is going to be closed, do nothing */
	if (client_is_expired(client))
		return;

	while (buflen > 0 && !client_is_expired(client)) {
		size_t left;

		assert(client->send_buf_size >= client->send_buf_used);
		left = client->send_buf_size - client->send_buf_used;

		copylen = buflen > left ? left : buflen;
		memcpy(client->send_buf + client->send_buf_used, buffer,
		       copylen);
		buflen -= copylen;
		client->send_buf_used += copylen;
		buffer += copylen;
		if (client->send_buf_used >= client->send_buf_size)
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

	buffer = xmalloc(length + 1);
	vsnprintf(buffer, length + 1, fmt, args);
	client_write(client, buffer, length);
	free(buffer);
}

mpd_fprintf void client_printf(struct client *client, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	client_vprintf(client, fmt, args);
	va_end(args);
}

static const char *const idle_names[] = {
	"database",
	"stored_playlist",
	"playlist",
	"player",
	"mixer",
	"output",
	"options",
};

/**
 * Send "idle" response to this client.
 */
static void
client_idle_notify(struct client *client)
{
	unsigned flags, i;

	assert(client->idle_waiting);
	assert(client->idle_flags != 0);

	flags = client->idle_flags;
	client->idle_flags = 0;
	client->idle_waiting = false;

	for (i = 0; i < sizeof(idle_names) / sizeof(idle_names[0]); ++i) {
		assert(idle_names[i] != NULL);

		if (flags & (1 << i))
			client_printf(client, "changed: %s\n",
				      idle_names[i]);
	}

	client_puts(client, "OK\n");
	client->lastTime = time(NULL);
}

void client_manager_idle_add(unsigned flags)
{
	struct client *client;

	assert(flags != 0);

	list_for_each_entry(client, &clients, siblings) {
		if (client_is_expired(client))
			continue;

		client->idle_flags |= flags;
		if (client->idle_waiting) {
			client_idle_notify(client);
			client_write_output(client);
		}
	}
}

bool client_idle_wait(struct client *client)
{
	assert(!client->idle_waiting);

	client->idle_waiting = true;

	if (client->idle_flags != 0) {
		client_idle_notify(client);
		return true;
	} else
		return false;
}
