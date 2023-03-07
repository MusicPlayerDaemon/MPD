// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Registry.hxx"
#include "NeighborPlugin.hxx"
#include "plugins/SmbclientNeighborPlugin.hxx"
#include "plugins/UpnpNeighborPlugin.hxx"
#include "plugins/UdisksNeighborPlugin.hxx"

#include <string.h>

constinit const NeighborPlugin *const neighbor_plugins[] = {
#ifdef ENABLE_SMBCLIENT
	&smbclient_neighbor_plugin,
#endif
#ifdef ENABLE_UPNP
	&upnp_neighbor_plugin,
#endif
#ifdef ENABLE_UDISKS
	&udisks_neighbor_plugin,
#endif
	nullptr
};

const NeighborPlugin *
GetNeighborPluginByName(const char *name) noexcept
{
	for (auto i = neighbor_plugins; *i != nullptr; ++i)
		if (strcmp((*i)->name, name) == 0)
			return *i;

	return nullptr;
}
