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

#include "zeroconf.h"
#include "zeroconf-internal.h"
#include "conf.h"
#include "config.h"

#include <glib.h>

/* The default service name to publish
 * (overridden by 'zeroconf_name' config parameter)
 */
#define SERVICE_NAME		"Music Player"

#define DEFAULT_ZEROCONF_ENABLED 1

static int zeroconfEnabled;

void initZeroconf(void)
{
	const char *serviceName = SERVICE_NAME;
	struct config_param *param;

	zeroconfEnabled = config_get_bool(CONF_ZEROCONF_ENABLED,
					  DEFAULT_ZEROCONF_ENABLED);
	if (!zeroconfEnabled)
		return;

	param = config_get_param(CONF_ZEROCONF_NAME);

	if (param && *param->value != 0)
		serviceName = param->value;

#ifdef HAVE_AVAHI
	init_avahi(serviceName);
#endif

#ifdef HAVE_BONJOUR
	init_zeroconf_osx(serviceName);
#endif
}

void finishZeroconf(void)
{
	if (!zeroconfEnabled)
		return;

#ifdef HAVE_AVAHI
	avahi_finish();
#endif /* HAVE_AVAHI */

#ifdef HAVE_BONJOUR
	bonjour_finish();
#endif
}
