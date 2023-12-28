// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <upnp.h>

UpnpClient_Handle
UpnpClientGlobalInit(const char* iface);

void
UpnpClientGlobalFinish() noexcept;
