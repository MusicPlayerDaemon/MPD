// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPNP_INIT_HXX
#define MPD_UPNP_INIT_HXX

void
UpnpGlobalInit(const char* iface);

void
UpnpGlobalFinish() noexcept;

#endif
