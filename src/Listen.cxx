/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "client/Listener.hxx"
#include "config/Param.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/ServerSocket.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
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

static ClientListener *listen_socket;
int listen_port;

/**
 * Throws #std::runtime_error on error.
 */
static void
listen_add_config_param(unsigned int port,
			const ConfigParam *param)
{
	assert(param != nullptr);

	if (0 == strcmp(param->value.c_str(), "any")) {
		listen_socket->AddPort(port);
	} else if (param->value[0] == '/' || param->value[0] == '~') {
		listen_socket->AddPath(param->GetPath());
	} else {
		listen_socket->AddHost(param->value.c_str(), port);
	}
}

#ifdef ENABLE_SYSTEMD_DAEMON

static bool
listen_systemd_activation()
{
	int n = sd_listen_fds(true);
	if (n <= 0) {
		if (n < 0)
			throw MakeErrno(-n, "sd_listen_fds() failed");
		return false;
	}

	for (int i = SD_LISTEN_FDS_START, end = SD_LISTEN_FDS_START + n;
	     i != end; ++i)
		listen_socket->AddFD(i);

	return true;
}

#endif

void
listen_global_init(EventLoop &loop, Partition &partition)
{
	int port = config_get_positive(ConfigOption::PORT, DEFAULT_PORT);
	const auto *param = config_get_param(ConfigOption::BIND_TO_ADDRESS);

	listen_socket = new ClientListener(loop, partition);

#ifdef ENABLE_SYSTEMD_DAEMON
	if (listen_systemd_activation())
		return;
#endif

	if (param != nullptr) {
		/* "bind_to_address" is configured, create listeners
		   for all values */

		do {
			try {
				listen_add_config_param(port, param);
			} catch (...) {
				delete listen_socket;
				std::throw_with_nested(FormatRuntimeError("Failed to listen on %s (line %i)",
									  param->value.c_str(),
									  param->line));
			}
		} while ((param = param->next) != nullptr);
	} else {
		/* no "bind_to_address" configured, bind the
		   configured port on all interfaces */

		try {
			listen_socket->AddPort(port);
		} catch (...) {
			delete listen_socket;
			std::throw_with_nested(FormatRuntimeError("Failed to listen on *:%d: ", port));
		}
	}

	try {
		listen_socket->Open();
	} catch (...) {
		delete listen_socket;
		throw;
	}

	listen_port = port;
}

void listen_global_finish(void)
{
	LogDebug(listen_domain, "listen_global_finish called");

	assert(listen_socket != nullptr);

	delete listen_socket;
}
