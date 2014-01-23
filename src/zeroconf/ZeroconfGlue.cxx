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
#include "ZeroconfGlue.hxx"
#include "ZeroconfAvahi.hxx"
#include "ZeroconfBonjour.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "Listen.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "Compiler.h"

static constexpr Domain zeroconf_domain("zeroconf");

/* The default service name to publish
 * (overridden by 'zeroconf_name' config parameter)
 */
#define SERVICE_NAME		"Music Player"

#define DEFAULT_ZEROCONF_ENABLED 1

static int zeroconfEnabled;

void
ZeroconfInit(gcc_unused EventLoop &loop)
{
	const char *serviceName;

	zeroconfEnabled = config_get_bool(CONF_ZEROCONF_ENABLED,
					  DEFAULT_ZEROCONF_ENABLED);
	if (!zeroconfEnabled)
		return;

	if (listen_port <= 0) {
		LogWarning(zeroconf_domain,
			   "No global port, disabling zeroconf");
		zeroconfEnabled = false;
		return;
	}

	serviceName = config_get_string(CONF_ZEROCONF_NAME, SERVICE_NAME);

#ifdef HAVE_AVAHI
	AvahiInit(loop, serviceName);
#endif

#ifdef HAVE_BONJOUR
	BonjourInit(loop, serviceName);
#endif
}

void
ZeroconfDeinit()
{
	if (!zeroconfEnabled)
		return;

#ifdef HAVE_AVAHI
	AvahiDeinit();
#endif /* HAVE_AVAHI */

#ifdef HAVE_BONJOUR
	BonjourDeinit();
#endif
}
