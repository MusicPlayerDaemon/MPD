/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "config/Net.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "fs/AllocatedPath.hxx"

#include <string.h>
#include <assert.h>

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

#define DEFAULT_PORT	6600

int listen_port;

/**
 * Throws #std::runtime_error on error.
 */
static void
listen_add_config_param(ClientListener &listener,
			unsigned int port,
			const ConfigParam *param)
{
	assert(param != nullptr);

	ServerSocketAddGeneric(listener, param->value.c_str(), port);
}

#ifdef ENABLE_SYSTEMD_DAEMON

static bool
listen_systemd_activation(ClientListener &listener)
{
	int n = sd_listen_fds(true);
	if (n <= 0) {
		if (n < 0)
			throw MakeErrno(-n, "sd_listen_fds() failed");
		return false;
	}

	for (int i = SD_LISTEN_FDS_START, end = SD_LISTEN_FDS_START + n;
	     i != end; ++i)
		listener.AddFD(i);

	return true;
}

#endif

void
listen_global_init(const ConfigData &config, ClientListener &listener)
{
	int port = config.GetPositive(ConfigOption::PORT, DEFAULT_PORT);

#ifdef ENABLE_SYSTEMD_DAEMON
	if (listen_systemd_activation(listener))
		return;
#endif

	for (const auto &param : config.GetParamList(ConfigOption::BIND_TO_ADDRESS)) {
		try {
			listen_add_config_param(listener, port, &param);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to listen on %s (line %i)",
								  param.value.c_str(),
								  param.line));
		}
	}

	if (listener.IsEmpty()) {
		/* no "bind_to_address" configured, bind the
		   configured port on all interfaces */

		try {
			listener.AddPort(port);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to listen on *:%d: ", port));
		}
	}

	listener.Open();

	listen_port = port;
}
