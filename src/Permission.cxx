/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "config.h"
#include "Permission.hxx"
#include "config/Param.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "util/IterableSplitString.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"

#include <cassert>
#include <cstring>
#include <map>
#include <numeric>
#include <string>
#include <utility>

static constexpr char PERMISSION_PASSWORD_CHAR = '@';
static constexpr char PERMISSION_SEPARATOR = ',';

static constexpr struct {
	const char *name;
	unsigned value;
} permission_names[] = {
	{ "read", PERMISSION_READ },
	{ "add", PERMISSION_ADD },
	{ "control", PERMISSION_CONTROL },
	{ "admin", PERMISSION_ADMIN },
	{ nullptr, 0 },
};

static std::map<std::string, unsigned> permission_passwords;

static unsigned permission_default;

#ifdef HAVE_UN
static unsigned local_permissions;
#endif

static unsigned
ParsePermission(StringView s)
{
	for (auto i = permission_names; i->name != nullptr; ++i)
		if (s.Equals(i->name))
			return i->value;

	throw FormatRuntimeError("unknown permission \"%.*s\"",
				 int(s.size), s.data);
}

static unsigned parsePermissions(const char *string)
{
	assert(string != nullptr);

	auto perms = IterableSplitString(string, PERMISSION_SEPARATOR);
	return std::accumulate(perms.begin(), perms.end(), 0,
			       [](const auto &perm, const auto &i) {
				       return perm | ParsePermission(i);
			       });
}

void
initPermissions(const ConfigData &config)
{
	permission_default = PERMISSION_READ | PERMISSION_ADD |
	    PERMISSION_CONTROL | PERMISSION_ADMIN;

	for (const auto &param : config.GetParamList(ConfigOption::PASSWORD)) {
		permission_default = 0;

		param.With([](const char *value){
			const char *separator = std::strchr(value,
						       PERMISSION_PASSWORD_CHAR);

			if (separator == nullptr)
				throw FormatRuntimeError("\"%c\" not found in password string",
							 PERMISSION_PASSWORD_CHAR);

			std::string password(value, separator);

			unsigned permission = parsePermissions(separator + 1);
			permission_passwords.insert(std::make_pair(std::move(password),
								   permission));
		});
	}

	config.With(ConfigOption::DEFAULT_PERMS, [](const char *value){
		if (value != nullptr)
			permission_default = parsePermissions(value);
	});

#ifdef HAVE_UN
	local_permissions = config.With(ConfigOption::LOCAL_PERMISSIONS, [](const char *value){
		return value != nullptr
			? parsePermissions(value)
			: permission_default;
	});
#endif
}

int
getPermissionFromPassword(const char *password, unsigned *permission) noexcept
{
	auto i = permission_passwords.find(password);
	if (i == permission_passwords.end())
		return -1;

	*permission = i->second;
	return 0;
}

unsigned
getDefaultPermissions() noexcept
{
	return permission_default;
}

#ifdef HAVE_UN

unsigned
GetLocalPermissions() noexcept
{
	return local_permissions;
}

#endif
