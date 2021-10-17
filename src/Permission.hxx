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

#ifndef MPD_PERMISSION_HXX
#define MPD_PERMISSION_HXX

#include "config.h"

#include <optional>

struct ConfigData;
class SocketAddress;

static constexpr unsigned PERMISSION_NONE = 0;
static constexpr unsigned PERMISSION_READ = 1;
static constexpr unsigned PERMISSION_ADD = 2;
static constexpr unsigned PERMISSION_CONTROL = 4;
static constexpr unsigned PERMISSION_ADMIN = 8;
static constexpr unsigned PERMISSION_PLAYER = 16;

/**
 * @return the permissions for the given password or std::nullopt if
 * the password is not accepted
 */
[[gnu::pure]]
std::optional<unsigned>
GetPermissionFromPassword(const char *password) noexcept;

[[gnu::const]]
unsigned
getDefaultPermissions() noexcept;

#ifdef HAVE_UN
[[gnu::const]]
unsigned
GetLocalPermissions() noexcept;
#endif

#ifdef HAVE_TCP
[[gnu::pure]]
int
GetPermissionsFromAddress(SocketAddress address) noexcept;
#endif

void
initPermissions(const ConfigData &config);

#endif
