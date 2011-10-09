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
#include "ntp_server.h"
#include "signals.h"
#include "io_thread.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

void
on_quit(void)
{
	io_thread_quit();
}

int
main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	g_thread_init(NULL);
	signals_init();
	io_thread_init();

	struct ntp_server ntp;
	ntp_server_init(&ntp);

	GError *error = NULL;
	if (!ntp_server_open(&ntp, &error)) {
		io_thread_deinit();
		g_printerr("%s\n", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	io_thread_run();

	ntp_server_close(&ntp);
	io_thread_deinit();
	return EXIT_SUCCESS;
}
