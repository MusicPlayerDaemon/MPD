// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPNP_UTIL_HXX
#define MPD_UPNP_UTIL_HXX

#include <string>

void
trimstring(std::string &s, const char *ws = " \t\n") noexcept;

std::string
path_getfather(const std::string &s) noexcept;

#endif /* _UPNPP_H_X_INCLUDED_ */
