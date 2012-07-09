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
#include "listen.h"
#include "server_socket.h"
#include "client.h"
#include "conf.h"
#include "main.h"

#include <string.h>
#include <assert.h>

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "listen"

#define DEFAULT_PORT	6600

static struct server_socket *listen_socket;
int listen_port;

static void
listen_callback(int fd, const struct sockaddr *address,
		size_t address_length, int uid, G_GNUC_UNUSED void *ctx)
{
	client_new(global_player_control, fd, address, address_length, uid);
}

static bool
listen_add_config_param(unsigned int port,
			const struct config_param *param,
			GError **error_r)
{
	assert(param != NULL);

	if (0 == strcmp(param->value, "any")) {
		return server_socket_add_port(listen_socket, port, error_r);
	} else if (param->value[0] == '/') {
		return server_socket_add_path(listen_socket, param->value,
					      error_r);
	} else {
		return server_socket_add_host(listen_socket, param->value,
					      port, error_r);
	}
}

static bool
listen_systemd_activation(GError **error_r)
{
#ifdef ENABLE_SYSTEMD_DAEMON
	int n = sd_listen_fds(true);
	if (n <= 0) {
		if (n < 0)
			g_warning("sd_listen_fds() failed: %s",
				  g_strerror(-n));
		return false;
	}

	for (int i = SD_LISTEN_FDS_START, end = SD_LISTEN_FDS_START + n;
	     i != end; ++i)
		if (!server_socket_add_fd(listen_socket, i, error_r))
			return false;

	return true;
#else
	(void)error_r;
	return false;
#endif
}

bool
listen_global_init(GError **error_r)
{
	int port = config_get_positive(CONF_PORT, DEFAULT_PORT);
	const struct config_param *param =
		config_get_next_param(CONF_BIND_TO_ADDRESS, NULL);
	bool success;
	GError *error = NULL;

	listen_socket = server_socket_new(listen_callback, NULL);

	if (listen_systemd_activation(&error))
		return true;

	if (error != NULL) {
		g_propagate_error(error_r, error);
		return false;
	}

	if (param != NULL) {
		/* "bind_to_address" is configured, create listeners
		   for all values */

		do {
			success = listen_add_config_param(port, param, &error);
			if (!success) {
				g_propagate_prefixed_error(error_r, error,
							   "Failed to listen on %s (line %i): ",
							   param->value, param->line);
				return false;
			}

			param = config_get_next_param(CONF_BIND_TO_ADDRESS,
						      param);
		} while (param != NULL);
	} else {
		/* no "bind_to_address" configured, bind the
		   configured port on all interfaces */

		success = server_socket_add_port(listen_socket, port, error_r);
		if (!success) {
			g_propagate_prefixed_error(error_r, error,
						   "Failed to listen on *:%d: ",
						   port);
			return false;
		}
	}

	if (!server_socket_open(listen_socket, error_r))
		return false;

	listen_port = port;
	return true;
}

void listen_global_finish(void)
{
	g_debug("listen_global_finish called");

	assert(listen_socket != NULL);

	server_socket_free(listen_socket);
}
