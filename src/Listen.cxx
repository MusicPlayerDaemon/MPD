/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "Log.hxx"
#include "client/Listener.hxx"
#include "config/Param.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "config/Net.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketUtil.hxx"
#include "system/Error.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/StandardDirectory.hxx"
#include "fs/XDG.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"

#include <sys/stat.h>

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

#define DEFAULT_PORT	6600

#if defined(USE_XDG) && defined(HAVE_UN)
static constexpr Domain listen_domain("listen");
#endif

int listen_port;

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
		listener.AddFD(UniqueSocketDescriptor(i));

	return true;
}

#endif

/**
 * Listen on "$XDG_RUNTIME_DIR/mpd/socket" (if applicable).
 *
 * @return true if a listener socket was added
 */
static bool
ListenXdgRuntimeDir(ClientListener &listener) noexcept
{
#if defined(USE_XDG) && defined(HAVE_UN)
	if (geteuid() == 0)
		/* this MPD instance is a system-wide daemon; don't
		   use $XDG_RUNTIME_DIR */
		return false;

	const auto mpd_runtime_dir = GetAppRuntimeDir();
	if (mpd_runtime_dir.IsNull())
		return false;

	const auto socket_path = mpd_runtime_dir / Path::FromFS("socket");
	unlink(socket_path.c_str());

	AllocatedSocketAddress address;
	address.SetLocal(socket_path.c_str());

	try {
		auto fd = socket_bind_listen(AF_LOCAL, SOCK_STREAM, 0,
					     address, 5);
		chmod(socket_path.c_str(), 0600);
		listener.AddFD(std::move(fd), std::move(address));
		return true;
	} catch (...) {
		FmtError(listen_domain,
			 "Failed to listen on '{}' (not fatal): {}",
			 socket_path, std::current_exception());
		return false;
	}
#else
	(void)listener;
	return false;
#endif
}

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
			ServerSocketAddGeneric(listener, param.value.c_str(),
					       port);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to listen on %s (line %i)",
								  param.value.c_str(),
								  param.line));
		}
	}

	bool have_xdg_runtime_listener = false;

	if (listener.IsEmpty()) {
		/* no "bind_to_address" configured, bind the
		   configured port on all interfaces */

		have_xdg_runtime_listener = ListenXdgRuntimeDir(listener);

		try {
			listener.AddPort(port);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to listen on *:%d: ", port));
		}
	}

	try {
		listener.Open();
	} catch (...) {
		if (have_xdg_runtime_listener)
			LogError(std::current_exception(),
				 "Default TCP listener setup failed, but this is okay because we have a $XDG_RUNTIME_DIR listener");
		else
			throw;
	}

	listen_port = port;
}
