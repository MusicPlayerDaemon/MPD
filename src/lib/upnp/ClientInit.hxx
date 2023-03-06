// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPNP_CLIENT_INIT_HXX
#define MPD_UPNP_CLIENT_INIT_HXX

#include "Compat.hxx"

UpnpClient_Handle
UpnpClientGlobalInit(const char* iface);

void
UpnpClientGlobalFinish() noexcept;

#endif
