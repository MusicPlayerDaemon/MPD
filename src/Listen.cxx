/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Listen.hxx"
#include "client/Client.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "event/ServerSocket.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "fs/AllocatedPath.hxx"
#include "Log.hxx"

#include <string.h>
#include <assert.h>

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

static constexpr Domain listen_domain("listen");

#define DEFAULT_PORT	6600

class ClientListener final : public ServerSocket {
	Partition &partition;

public:
	ClientListener(EventLoop &_loop, Partition &_partition)
		:ServerSocket(_loop), partition(_partition) {}

private:
	virtual void OnAccept(int fd, const sockaddr &address,
			      size_t address_length, int uid) {
		client_new(GetEventLoop(), partition,
			   fd, &address, address_length, uid);
	}
};

static ClientListener *listen_socket;
int listen_port;

static bool
listen_add_config_param(unsigned int port,
			const struct config_param *param,
			Error &error_r)
{
	assert(param != nullptr);

	if (0 == strcmp(param->value.c_str(), "any")) {
		return listen_socket->AddPort(port, error_r);
	} else if (param->value[0] == '/' || param->value[0] == '~') {
		auto path = config_parse_path(param, error_r);
		return !path.IsNull() &&
			listen_socket->AddPath(std::move(path), error_r);
	} else {
		return listen_socket->AddHost(param->value.c_str(), port,
					      error_r);
	}
}

#ifdef ENABLE_SYSTEMD_DAEMON

static bool
listen_systemd_activation(Error &error_r)
{
	int n = sd_listen_fds(true);
	if (n <= 0) {
		if (n < 0)
			FormatErrno(listen_domain, -n,
				    "sd_listen_fds() failed");
		return false;
	}

	for (int i = SD_LISTEN_FDS_START, end = SD_LISTEN_FDS_START + n;
	     i != end; ++i)
		if (!listen_socket->AddFD(i, error_r))
			return false;

	return true;
}

#endif

bool
listen_global_init(EventLoop &loop, Partition &partition, Error &error)
{
	int port = config_get_positive(CONF_PORT, DEFAULT_PORT);
	const struct config_param *param =
		config_get_param(CONF_BIND_TO_ADDRESS);

	listen_socket = new ClientListener(loop, partition);

#ifdef ENABLE_SYSTEMD_DAEMON
	if (listen_systemd_activation(error))
		return true;

	if (error.IsDefined())
		return false;
#endif

	if (param != nullptr) {
		/* "bind_to_address" is configured, create listeners
		   for all values */

		do {
			if (!listen_add_config_param(port, param, error)) {
				delete listen_socket;
				error.FormatPrefix("Failed to listen on %s (line %i): ",
						   param->value.c_str(),
						   param->line);
				return false;
			}
		} while ((param = param->next) != nullptr);
	} else {
		/* no "bind_to_address" configured, bind the
		   configured port on all interfaces */

		if (!listen_socket->AddPort(port, error)) {
			delete listen_socket;
			error.FormatPrefix("Failed to listen on *:%d: ", port);
			return false;
		}
	}

	if (!listen_socket->Open(error)) {
		delete listen_socket;
		return false;
	}

	listen_port = port;
	return true;
}

void listen_global_finish(void)
{
	LogDebug(listen_domain, "listen_global_finish called");

	assert(listen_socket != nullptr);

	delete listen_socket;
}
