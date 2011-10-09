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

#include "config.h"
#include "resolver.h"
#include "io_thread.h"
#include "tcp_connect.h"
#include "fd_util.h"

#include <assert.h>
#include <stdlib.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

static struct tcp_connect *handle;
static GMutex *mutex;
static GCond *cond;
static bool done, success;

static void
my_tcp_connect_success(int fd, G_GNUC_UNUSED void *ctx)
{
	assert(!done);
	assert(!success);

	close_socket(fd);
	g_print("success\n");

	g_mutex_lock(mutex);
	done = success = true;
	g_cond_signal(cond);
	g_mutex_unlock(mutex);
}

static void
my_tcp_connect_error(GError *error, G_GNUC_UNUSED void *ctx)
{
	assert(!done);
	assert(!success);

	g_printerr("error: %s\n", error->message);
	g_error_free(error);

	g_mutex_lock(mutex);
	done = true;
	g_cond_signal(cond);
	g_mutex_unlock(mutex);
}

static void
my_tcp_connect_timeout(G_GNUC_UNUSED void *ctx)
{
	assert(!done);
	assert(!success);

	g_printerr("timeout\n");

	g_mutex_lock(mutex);
	done = true;
	g_cond_signal(cond);
	g_mutex_unlock(mutex);
}

static void
my_tcp_connect_canceled(G_GNUC_UNUSED void *ctx)
{
	assert(!done);
	assert(!success);

	g_printerr("canceled\n");

	g_mutex_lock(mutex);
	done = true;
	g_cond_signal(cond);
	g_mutex_unlock(mutex);
}

static const struct tcp_connect_handler my_tcp_connect_handler = {
	.success = my_tcp_connect_success,
	.error = my_tcp_connect_error,
	.timeout = my_tcp_connect_timeout,
	.canceled = my_tcp_connect_canceled,
};

int main(int argc, char **argv)
{
	if (argc != 2) {
		g_printerr("Usage: run_tcp_connect IP:PORT\n");
		return 1;
	}

	GError *error = NULL;
	struct addrinfo *ai = resolve_host_port(argv[1], 80, 0, SOCK_STREAM,
						&error);
	if (ai == NULL) {
		g_printerr("%s\n", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	/* initialize GLib */

	g_thread_init(NULL);

	/* initialize MPD */

	io_thread_init();
	if (!io_thread_start(&error)) {
		freeaddrinfo(ai);
		g_printerr("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	/* open the connection */

	mutex = g_mutex_new();
	cond = g_cond_new();

	tcp_connect_address(ai->ai_addr, ai->ai_addrlen, 5000,
			    &my_tcp_connect_handler, NULL,
			    &handle);
	freeaddrinfo(ai);

	if (handle != NULL) {
		g_mutex_lock(mutex);
		while (!done)
			g_cond_wait(cond, mutex);
		g_mutex_unlock(mutex);

		tcp_connect_free(handle);
	}

	g_cond_free(cond);
	g_mutex_free(mutex);

	/* deinitialize everything */

	io_thread_deinit();

	return EXIT_SUCCESS;
}
