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

#ifndef MPD_UPNP_ACTION_HXX
#define MPD_UPNP_ACTION_HXX

#include "util/Compiler.h"

#include <upnptools.h>

static constexpr unsigned
CountNameValuePairs() noexcept
{
	return 0;
}

template<typename... Args>
static constexpr unsigned
CountNameValuePairs([[maybe_unused]] const char *name, [[maybe_unused]] const char *value,
		    Args... args) noexcept
{
	return 1 + CountNameValuePairs(args...);
}

#ifdef USING_PUPNP
/**
 * A wrapper for UpnpMakeAction() that counts the number of name/value
 * pairs and adds the nullptr sentinel.
 */
template<typename... Args>
static inline IXML_Document *
MakeActionHelper(const char *action_name, const char *service_type,
		 Args... args) noexcept
{
	const unsigned n = CountNameValuePairs(args...);
	return UpnpMakeAction(action_name, service_type, n,
			      args...,
			      nullptr, nullptr);
}
#endif

#endif
