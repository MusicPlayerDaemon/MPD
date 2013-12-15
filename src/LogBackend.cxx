/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Log.hxx"
#include "util/Domain.hxx"

#include <glib.h>

#include <assert.h>

static GLogLevelFlags
ToGLib(LogLevel level)
{
	switch (level) {
	case LogLevel::DEBUG:
		return G_LOG_LEVEL_DEBUG;

	case LogLevel::INFO:
		return G_LOG_LEVEL_INFO;

	case LogLevel::DEFAULT:
		return G_LOG_LEVEL_MESSAGE;

	case LogLevel::WARNING:
	case LogLevel::ERROR:
		return G_LOG_LEVEL_WARNING;
	}

	assert(false);
	gcc_unreachable();
}

void
Log(const Domain &domain, LogLevel level, const char *msg)
{
	g_log(domain.GetName(), ToGLib(level), "%s", msg);
}
