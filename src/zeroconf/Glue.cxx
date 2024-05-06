// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Glue.hxx"
#include "Helper.hxx"
#include "avahi/Helper.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "Listen.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <climits>

#include <string.h>
#include <unistd.h>

#ifndef HOST_NAME_MAX
/* HOST_NAME_MAX is not a portable macro; it is undefined on some
   systems */
#define HOST_NAME_MAX 255
#endif

static constexpr Domain zeroconf_domain("zeroconf");

/* The default service name to publish
 * (overridden by 'zeroconf_name' config parameter)
 */
#define SERVICE_NAME		"Music Player @ %h"

/* The dns-sd service type qualifier to publish */
#define SERVICE_TYPE		"_mpd._tcp"

#define DEFAULT_ZEROCONF_ENABLED 1

std::unique_ptr<ZeroconfHelper>
ZeroconfInit(const ConfigData &config, [[maybe_unused]] EventLoop &loop)
{
	const char *serviceName;

	if (!config.GetBool(ConfigOption::ZEROCONF_ENABLED,
			    DEFAULT_ZEROCONF_ENABLED))
		return nullptr;

	if (listen_port <= 0) {
		LogWarning(zeroconf_domain,
			   "No global port, disabling zeroconf");
		return nullptr;
	}

	serviceName = config.GetString(ConfigOption::ZEROCONF_NAME,
				       SERVICE_NAME);

	/* replace "%h" with the host name */
	const char *h = strstr(serviceName, "%h");
	std::string buffer;
	if (h != nullptr) {
		char hostname[HOST_NAME_MAX+1];
		if (gethostname(hostname, HOST_NAME_MAX) == 0) {
			buffer = serviceName;
			buffer.replace(h - serviceName, 2, hostname);
			serviceName = buffer.c_str();
		}
	}

	return std::make_unique<ZeroconfHelper>(loop, serviceName,
						SERVICE_TYPE, listen_port);
}
