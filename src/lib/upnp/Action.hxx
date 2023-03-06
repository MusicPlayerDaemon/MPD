// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPNP_ACTION_HXX
#define MPD_UPNP_ACTION_HXX

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
