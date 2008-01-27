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

#include "interface.h"
#include "command.h"
#include "conf.h"
#include "list.h"
#include "log.h"
#include "listen.h"
#include "playlist.h"
#include "permission.h"
#include "sllist.h"
#include "utils.h"
#include "ioops.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define GREETING				"OK MPD " PROTOCOL_VERSION "\n"

#define INTERFACE_MAX_BUFFER_LENGTH			(40960)
#define INTERFACE_LIST_MODE_BEGIN			"command_list_begin"
#define INTERFACE_LIST_OK_MODE_BEGIN			"command_list_ok_begin"
#define INTERFACE_LIST_MODE_END				"command_list_end"
#define INTERFACE_DEFAULT_OUT_BUFFER_SIZE		(4096)
#define INTERFACE_TIMEOUT_DEFAULT			(60)
#define INTERFACE_MAX_CONNECTIONS_DEFAULT		(10)
#define INTERFACE_MAX_COMMAND_LIST_DEFAULT		(2048*1024)
#define INTERFACE_MAX_OUTPUT_BUFFER_SIZE_DEFAULT	(8192*1024)

/* set this to zero to indicate we have no possible interfaces */
static int interface_max_connections;	/*INTERFACE_MAX_CONNECTIONS_DEFAULT; */
static int interface_timeout = INTERFACE_TIMEOUT_DEFAULT;
static size_t interface_max_command_list_size =
    INTERFACE_MAX_COMMAND_LIST_DEFAULT;
static size_t interface_max_output_buffer_size =
    INTERFACE_MAX_OUTPUT_BUFFER_SIZE_DEFAULT;

/* List of registered external IO handlers */
static struct ioOps *ioList;

/* maybe make conf option for this, or... 32 might be good enough */
static long int interface_list_cache_size = 32;

/* shared globally between all interfaces: */
static struct strnode *list_cache;
static struct strnode *list_cache_head;
static struct strnode *list_cache_tail;

typedef struct _Interface {
	char buffer[INTERFACE_MAX_BUFFER_LENGTH];
	int bufferLength;
	int bufferPos;
	int fd;	/* file descriptor */
	int permission;
	time_t lastTime;
	struct strnode *cmd_list;	/* for when in list mode */
	struct strnode *cmd_list_tail;	/* for when in list mode */
	int cmd_list_OK;	/* print OK after each command execution */
	int cmd_list_size;	/* mem cmd_list consumes */
	int cmd_list_dup;	/* has the cmd_list been copied to private space? */
	struct sllnode *deferred_send;	/* for output if client is slow */
	int deferred_bytes;	/* mem deferred_send consumes */
	int expired;	/* set whether this interface should be closed on next
			   check of old interfaces */
	int num;	/* interface number */

	char *send_buf;
	int send_buf_used;	/* bytes used this instance */
	int send_buf_size;	/* bytes usable this instance */
	int send_buf_alloc;	/* bytes actually allocated */
} Interface;

static Interface *interfaces;

static void flushInterfaceBuffer(Interface * interface);

static void printInterfaceOutBuffer(Interface * interface);

#ifdef SO_SNDBUF
static int get_default_snd_buf_size(Interface * interface)
{
	int new_size;
	socklen_t sockOptLen = sizeof(int);

	if (getsockopt(interface->fd, SOL_SOCKET, SO_SNDBUF,
		       (char *)&new_size, &sockOptLen) < 0) {
		DEBUG("problem getting sockets send buffer size\n");
		return INTERFACE_DEFAULT_OUT_BUFFER_SIZE;
	}
	if (new_size > 0)
		return new_size;
	DEBUG("sockets send buffer size is not positive\n");
	return INTERFACE_DEFAULT_OUT_BUFFER_SIZE;
}
#else /* !SO_SNDBUF */
static int get_default_snd_buf_size(Interface * interface)
{
	return INTERFACE_DEFAULT_OUT_BUFFER_SIZE;
}
#endif /* !SO_SNDBUF */

static void set_send_buf_size(Interface * interface)
{
	int new_size = get_default_snd_buf_size(interface);
	if (interface->send_buf_size != new_size) {
		interface->send_buf_size = new_size;
		/* don't resize to get smaller, only bigger */
		if (interface->send_buf_alloc < new_size) {
			if (interface->send_buf)
				free(interface->send_buf);
			interface->send_buf = xmalloc(new_size);
			interface->send_buf_alloc = new_size;
		}
	}
}

static void openInterface(Interface * interface, int fd)
{
	int flags;

	assert(interface->fd < 0);

	interface->cmd_list_size = 0;
	interface->cmd_list_dup = 0;
	interface->cmd_list_OK = -1;
	interface->bufferLength = 0;
	interface->bufferPos = 0;
	interface->fd = fd;
	while ((flags = fcntl(fd, F_GETFL)) < 0 && errno == EINTR) ;
	while (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 && errno == EINTR) ;
	interface->lastTime = time(NULL);
	interface->cmd_list = NULL;
	interface->cmd_list_tail = NULL;
	interface->deferred_send = NULL;
	interface->expired = 0;
	interface->deferred_bytes = 0;
	interface->send_buf_used = 0;

	interface->permission = getDefaultPermissions();
	set_send_buf_size(interface);

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

static void cmd_list_clone(Interface * interface)
{
	struct strnode *new = dup_strlist(interface->cmd_list);
	free_cmd_list(interface->cmd_list);
	interface->cmd_list = new;
	interface->cmd_list_dup = 1;

	/* new tail */
	while (new && new->next)
		new = new->next;
	interface->cmd_list_tail = new;
}

static void new_cmd_list_ptr(Interface * interface, char *s, const int size)
{
	int i;
	struct strnode *new;

	if (!interface->cmd_list_dup) {
		for (i = interface_list_cache_size - 1; i >= 0; --i) {
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
	new = interface->cmd_list_dup ? new_strnode_dup(s, size)
	                              : new_strnode(s);
out:
	if (interface->cmd_list) {
		interface->cmd_list_tail->next = new;
		interface->cmd_list_tail = new;
	} else
		interface->cmd_list = interface->cmd_list_tail = new;
}

static void closeInterface(Interface * interface)
{
	struct sllnode *buf;
	if (interface->fd < 0)
		return;
	xclose(interface->fd);
	interface->fd = -1;

	if (interface->cmd_list) {
		free_cmd_list(interface->cmd_list);
		interface->cmd_list = NULL;
	}

	if ((buf = interface->deferred_send)) {
		do {
			struct sllnode *prev = buf;
			buf = buf->next;
			free(prev);
		} while (buf);
		interface->deferred_send = NULL;
	}

	SECURE("interface %i: closed\n", interface->num);
}

void openAInterface(int fd, struct sockaddr *addr)
{
	int i;

	for (i = 0; i < interface_max_connections
	     && interfaces[i].fd >= 0; i++) /* nothing */ ;

	if (i == interface_max_connections) {
		ERROR("Max Connections Reached!\n");
		xclose(fd);
	} else {
		SECURE("interface %i: opened from ", i);
		switch (addr->sa_family) {
		case AF_INET:
			{
				char *host = inet_ntoa(((struct sockaddr_in *)
							addr)->sin_addr);
				if (host) {
					SECURE("%s\n", host);
				} else {
					SECURE("error getting ipv4 address\n");
				}
			}
			break;
#ifdef HAVE_IPV6
		case AF_INET6:
			{
				char host[INET6_ADDRSTRLEN + 1];
				memset(host, 0, INET6_ADDRSTRLEN + 1);
				if (inet_ntop(AF_INET6, (void *)
					      &(((struct sockaddr_in6 *)addr)->
						sin6_addr), host,
					      INET6_ADDRSTRLEN)) {
					SECURE("%s\n", host);
				} else {
					SECURE("error getting ipv6 address\n");
				}
			}
			break;
#endif
		case AF_UNIX:
			SECURE("local connection\n");
			break;
		default:
			SECURE("unknown\n");
		}
		openInterface(&(interfaces[i]), fd);
	}
}

static int processLineOfInput(Interface * interface)
{
	int ret = 1;
	char *line = interface->buffer + interface->bufferPos;

	if (interface->cmd_list_OK >= 0) {
		if (strcmp(line, INTERFACE_LIST_MODE_END) == 0) {
			DEBUG("interface %i: process command "
			      "list\n", interface->num);
			ret = processListOfCommands(interface->fd,
						    &(interface->permission),
						    &(interface->expired),
						    interface->cmd_list_OK,
						    interface->cmd_list);
			DEBUG("interface %i: process command "
			      "list returned %i\n", interface->num, ret);
			if (ret == 0)
				commandSuccess(interface->fd);
			else if (ret == COMMAND_RETURN_CLOSE
				 || interface->expired)
				closeInterface(interface);

			printInterfaceOutBuffer(interface);
			free_cmd_list(interface->cmd_list);
			interface->cmd_list = NULL;
			interface->cmd_list_OK = -1;
		} else {
			size_t len = strlen(line) + 1;
			interface->cmd_list_size += len;
			if (interface->cmd_list_size >
			    interface_max_command_list_size) {
				ERROR("interface %i: command "
				      "list size (%i) is "
				      "larger than the max "
				      "(%li)\n",
				      interface->num,
				      interface->cmd_list_size,
				      (long)interface_max_command_list_size);
				closeInterface(interface);
				ret = COMMAND_RETURN_CLOSE;
			} else
				new_cmd_list_ptr(interface, line, len);
		}
	} else {
		if (strcmp(line, INTERFACE_LIST_MODE_BEGIN) == 0) {
			interface->cmd_list_OK = 0;
			ret = 1;
		} else if (strcmp(line, INTERFACE_LIST_OK_MODE_BEGIN) == 0) {
			interface->cmd_list_OK = 1;
			ret = 1;
		} else {
			DEBUG("interface %i: process command \"%s\"\n",
			      interface->num, line);
			ret = processCommand(interface->fd,
					     &(interface->permission), line);
			DEBUG("interface %i: command returned %i\n",
			      interface->num, ret);
			if (ret == 0)
				commandSuccess(interface->fd);
			else if (ret == COMMAND_RETURN_CLOSE
				 || interface->expired) {
				closeInterface(interface);
			}
			printInterfaceOutBuffer(interface);
		}
	}

	return ret;
}

static int processBytesRead(Interface * interface, int bytesRead)
{
	int ret = 0;
	char *buf_tail = &(interface->buffer[interface->bufferLength - 1]);

	while (bytesRead > 0) {
		interface->bufferLength++;
		bytesRead--;
		buf_tail++;
		if (*buf_tail == '\n') {
			*buf_tail = '\0';
			if (interface->bufferLength - interface->bufferPos > 1) {
				if (*(buf_tail - 1) == '\r')
					*(buf_tail - 1) = '\0';
			}
			ret = processLineOfInput(interface);
			if (interface->expired)
				return ret;
			interface->bufferPos = interface->bufferLength;
		}
		if (interface->bufferLength == INTERFACE_MAX_BUFFER_LENGTH) {
			if (interface->bufferPos == 0) {
				ERROR("interface %i: buffer overflow\n",
				      interface->num);
				closeInterface(interface);
				return 1;
			}
			if (interface->cmd_list_OK >= 0 &&
			    interface->cmd_list &&
			    !interface->cmd_list_dup)
				cmd_list_clone(interface);
			interface->bufferLength -= interface->bufferPos;
			memmove(interface->buffer,
				interface->buffer + interface->bufferPos,
				interface->bufferLength);
			interface->bufferPos = 0;
		}
		if (ret == COMMAND_RETURN_KILL || ret == COMMAND_RETURN_CLOSE) {
			return ret;
		}

	}

	return ret;
}

static int interfaceReadInput(Interface * interface)
{
	int bytesRead;

	bytesRead = read(interface->fd,
			 interface->buffer + interface->bufferLength,
			 INTERFACE_MAX_BUFFER_LENGTH - interface->bufferLength);

	if (bytesRead > 0)
		return processBytesRead(interface, bytesRead);
	else if (bytesRead == 0 || (bytesRead < 0 && errno != EINTR)) {
		closeInterface(interface);
	} else
		return 0;

	return 1;
}

static void addInterfacesReadyToReadAndListenSocketToFdSet(fd_set * fds,
							   int *fdmax)
{
	int i;

	FD_ZERO(fds);
	addListenSocketsToFdSet(fds, fdmax);

	for (i = 0; i < interface_max_connections; i++) {
		if (interfaces[i].fd >= 0 && !interfaces[i].expired
		    && !interfaces[i].deferred_send) {
			FD_SET(interfaces[i].fd, fds);
			if (*fdmax < interfaces[i].fd)
				*fdmax = interfaces[i].fd;
		}
	}
}

static void addInterfacesForBufferFlushToFdSet(fd_set * fds, int *fdmax)
{
	int i;

	FD_ZERO(fds);

	for (i = 0; i < interface_max_connections; i++) {
		if (interfaces[i].fd >= 0 && !interfaces[i].expired
		    && interfaces[i].deferred_send) {
			FD_SET(interfaces[i].fd, fds);
			if (*fdmax < interfaces[i].fd)
				*fdmax = interfaces[i].fd;
		}
	}
}

static void closeNextErroredInterface(void)
{
	fd_set fds;
	struct timeval tv;
	int i;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	for (i = 0; i < interface_max_connections; i++) {
		if (interfaces[i].fd >= 0) {
			FD_ZERO(&fds);
			FD_SET(interfaces[i].fd, &fds);
			if (select(FD_SETSIZE, &fds, NULL, NULL, &tv) < 0) {
				closeInterface(&interfaces[i]);
				return;
			}
		}
	}
}

int doIOForInterfaces(void)
{
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	struct timeval tv;
	int i;
	int selret;
	int fdmax;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (1) {
		fdmax = 0;

		FD_ZERO( &rfds );
		FD_ZERO( &wfds );
		FD_ZERO( &efds );
		addInterfacesReadyToReadAndListenSocketToFdSet(&rfds, &fdmax);
		addInterfacesForBufferFlushToFdSet(&wfds, &fdmax);

		/* Add fds for all registered IO handlers */
		if( ioList ) {
			struct ioOps *o = ioList;
			while( o ) {
				struct ioOps *current = o;
				int fdnum;
				assert( current->fdset );
				fdnum = current->fdset( &rfds, &wfds, &efds );
				if( fdmax < fdnum )
					fdmax = fdnum;
				o = o->next;
			}
		}

		selret = select(fdmax + 1, &rfds, &wfds, &efds, &tv);

		if (selret < 0 && errno == EINTR)
			break;

		/* Consume fds for all registered IO handlers */
		if( ioList ) {
			struct ioOps *o = ioList;
			while( o ) {
				struct ioOps *current = o;
				assert( current->consume );
				selret = current->consume( selret, &rfds, &wfds, &efds );
				o = o->next;
			}
		}

		if (selret == 0)
			break;

		if (selret < 0) {
			closeNextErroredInterface();
			continue;
		}

		getConnections(&rfds);

		for (i = 0; i < interface_max_connections; i++) {
			if (interfaces[i].fd >= 0
			    && FD_ISSET(interfaces[i].fd, &rfds)) {
				if (COMMAND_RETURN_KILL ==
				    interfaceReadInput(&(interfaces[i]))) {
					return COMMAND_RETURN_KILL;
				}
				interfaces[i].lastTime = time(NULL);
			}
			if (interfaces[i].fd >= 0
			    && FD_ISSET(interfaces[i].fd, &wfds)) {
				flushInterfaceBuffer(&interfaces[i]);
				interfaces[i].lastTime = time(NULL);
			}
		}

		tv.tv_sec = 0;
		tv.tv_usec = 0;
	}

	return 1;
}

void initInterfaces(void)
{
	int i;
	char *test;
	ConfigParam *param;

	param = getConfigParam(CONF_CONN_TIMEOUT);

	if (param) {
		interface_timeout = strtol(param->value, &test, 10);
		if (*test != '\0' || interface_timeout <= 0) {
			FATAL("connection timeout \"%s\" is not a positive "
			      "integer, line %i\n", CONF_CONN_TIMEOUT,
			      param->line);
		}
	}

	param = getConfigParam(CONF_MAX_CONN);

	if (param) {
		interface_max_connections = strtol(param->value, &test, 10);
		if (*test != '\0' || interface_max_connections <= 0) {
			FATAL("max connections \"%s\" is not a positive integer"
			      ", line %i\n", param->value, param->line);
		}
	} else
		interface_max_connections = INTERFACE_MAX_CONNECTIONS_DEFAULT;

	param = getConfigParam(CONF_MAX_COMMAND_LIST_SIZE);

	if (param) {
		interface_max_command_list_size = strtol(param->value,
							 &test, 10);
		if (*test != '\0' || interface_max_command_list_size <= 0) {
			FATAL("max command list size \"%s\" is not a positive "
			      "integer, line %i\n", param->value, param->line);
		}
		interface_max_command_list_size *= 1024;
	}

	param = getConfigParam(CONF_MAX_OUTPUT_BUFFER_SIZE);

	if (param) {
		interface_max_output_buffer_size = strtol(param->value,
		                                          &test, 10);
		if (*test != '\0' || interface_max_output_buffer_size <= 0) {
			FATAL("max output buffer size \"%s\" is not a positive "
			      "integer, line %i\n", param->value, param->line);
		}
		interface_max_output_buffer_size *= 1024;
	}

	interfaces = xmalloc(sizeof(Interface) * interface_max_connections);

	list_cache = xcalloc(interface_list_cache_size, sizeof(struct strnode));
	list_cache_head = &(list_cache[0]);
	list_cache_tail = &(list_cache[interface_list_cache_size - 1]);

	for (i = 0; i < interface_max_connections; i++) {
		interfaces[i].fd = -1;
		interfaces[i].send_buf = NULL;
		interfaces[i].send_buf_size = 0;
		interfaces[i].send_buf_alloc = 0;
		interfaces[i].num = i;
	}
}

static void closeAllInterfaces(void)
{
	int i;

	for (i = 0; i < interface_max_connections; i++) {
		if (interfaces[i].fd > 0)
			closeInterface(&(interfaces[i]));
		if (interfaces[i].send_buf)
			free(interfaces[i].send_buf);
	}
	free(list_cache);
}

void freeAllInterfaces(void)
{
	closeAllInterfaces();

	free(interfaces);

	interface_max_connections = 0;
}

void closeOldInterfaces(void)
{
	int i;

	for (i = 0; i < interface_max_connections; i++) {
		if (interfaces[i].fd > 0) {
			if (interfaces[i].expired) {
				DEBUG("interface %i: expired\n", i);
				closeInterface(&(interfaces[i]));
			} else if (time(NULL) - interfaces[i].lastTime >
				   interface_timeout) {
				DEBUG("interface %i: timeout\n", i);
				closeInterface(&(interfaces[i]));
			}
		}
	}
}

static void flushInterfaceBuffer(Interface * interface)
{
	struct sllnode *buf;
	int ret = 0;

	buf = interface->deferred_send;
	while (buf) {
		ret = write(interface->fd, buf->data, buf->size);
		if (ret < 0)
			break;
		else if (ret < buf->size) {
			interface->deferred_bytes -= ret;
			buf->data = (char *)buf->data + ret;
			buf->size -= ret;
		} else {
			struct sllnode *tmp = buf;
			interface->deferred_bytes -= (buf->size +
						      sizeof(struct sllnode));
			buf = buf->next;
			free(tmp);
			interface->deferred_send = buf;
		}
		interface->lastTime = time(NULL);
	}

	if (!interface->deferred_send) {
		DEBUG("interface %i: buffer empty %i\n", interface->num,
		      interface->deferred_bytes);
		assert(interface->deferred_bytes == 0);
	} else if (ret < 0 && errno != EAGAIN && errno != EINTR) {
		/* cause interface to close */
		DEBUG("interface %i: problems flushing buffer\n",
		      interface->num);
		buf = interface->deferred_send;
		do {
			struct sllnode *prev = buf;
			buf = buf->next;
			free(prev);
		} while (buf);
		interface->deferred_send = NULL;
		interface->deferred_bytes = 0;
		interface->expired = 1;
	}
}

int interfacePrintWithFD(int fd, char *buffer, int buflen)
{
	static int i;
	int copylen;
	Interface *interface;

	assert(fd > 0);

	if (i >= interface_max_connections ||
	    interfaces[i].fd < 0 || interfaces[i].fd != fd) {
		for (i = 0; i < interface_max_connections; i++) {
			if (interfaces[i].fd == fd)
				break;
		}
		if (i == interface_max_connections)
			return -1;
	}

	/* if fd isn't found or interfaces is going to be closed, do nothing */
	if (interfaces[i].expired)
		return 0;

	interface = interfaces + i;

	while (buflen > 0 && !interface->expired) {
		int left = interface->send_buf_size - interface->send_buf_used;
		copylen = buflen > left ? left : buflen;
		memcpy(interface->send_buf + interface->send_buf_used, buffer,
		       copylen);
		buflen -= copylen;
		interface->send_buf_used += copylen;
		buffer += copylen;
		if (interface->send_buf_used >= interface->send_buf_size)
			printInterfaceOutBuffer(interface);
	}

	return 0;
}

static void printInterfaceOutBuffer(Interface * interface)
{
	int ret;
	struct sllnode *buf;

	if (interface->fd < 0 || interface->expired ||
	    !interface->send_buf_used)
		return;

	if ((buf = interface->deferred_send)) {
		interface->deferred_bytes += sizeof(struct sllnode)
		                             + interface->send_buf_used;
		if (interface->deferred_bytes >
		    interface_max_output_buffer_size) {
			ERROR("interface %i: output buffer size (%li) is "
			      "larger than the max (%li)\n",
			      interface->num,
			      (long)interface->deferred_bytes,
			      (long)interface_max_output_buffer_size);
			/* cause interface to close */
			interface->expired = 1;
			do {
				struct sllnode *prev = buf;
				buf = buf->next;
				free(prev);
			} while (buf);
			interface->deferred_send = NULL;
			interface->deferred_bytes = 0;
		} else {
			while (buf->next)
				buf = buf->next;
			buf->next = new_sllnode(interface->send_buf,
						interface->send_buf_used);
		}
	} else {
		if ((ret = write(interface->fd, interface->send_buf,
				 interface->send_buf_used)) < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				interface->deferred_send =
				    new_sllnode(interface->send_buf,
						interface->send_buf_used);
			} else {
				DEBUG("interface %i: problems writing\n",
				      interface->num);
				interface->expired = 1;
				return;
			}
		} else if (ret < interface->send_buf_used) {
			interface->deferred_send =
			    new_sllnode(interface->send_buf + ret,
					interface->send_buf_used - ret);
		}
		if (interface->deferred_send) {
			DEBUG("interface %i: buffer created\n", interface->num);
			interface->deferred_bytes =
			    interface->deferred_send->size
			    + sizeof(struct sllnode);
		}
	}

	interface->send_buf_used = 0;
}

/* From ioops.h: */
void registerIO( struct ioOps *ops )
{
	assert( ops != NULL );

	ops->next = ioList;
	ioList = ops;
	ops->prev = NULL;
	if( ops->next )
		ops->next->prev = ops;
}

void deregisterIO( struct ioOps *ops )
{
	assert( ops != NULL );

	if( ioList == ops )
		ioList = ops->next;
	else if( ops->prev != NULL )
		ops->prev->next = ops->next;
}
