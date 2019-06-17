/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_UPNP_COMPAT_HXX
#define MPD_UPNP_COMPAT_HXX

#include <upnp.h>

#if UPNP_VERSION < 10800
/* emulate the libupnp 1.8 API with older versions */

using UpnpDiscovery = Upnp_Discovery;

#endif

#if UPNP_VERSION < 10624
#include "util/Compiler.h"

gcc_pure
static inline int
UpnpDiscovery_get_Expires(const UpnpDiscovery *disco) noexcept
{
  return disco->Expires;
}

gcc_pure
static inline const char *
UpnpDiscovery_get_DeviceID_cstr(const UpnpDiscovery *disco) noexcept
{
  return disco->DeviceId;
}

gcc_pure
static inline const char *
UpnpDiscovery_get_DeviceType_cstr(const UpnpDiscovery *disco) noexcept
{
  return disco->DeviceType;
}

gcc_pure
static inline const char *
UpnpDiscovery_get_ServiceType_cstr(const UpnpDiscovery *disco) noexcept
{
  return disco->ServiceType;
}

gcc_pure
static inline const char *
UpnpDiscovery_get_Location_cstr(const UpnpDiscovery *disco) noexcept
{
  return disco->Location;
}

#endif

#endif
