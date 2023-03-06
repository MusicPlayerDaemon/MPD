// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
