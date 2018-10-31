/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "util/RuntimeError.hxx"

#include <algorithm>
#include <map>
#include <string>

#include <assert.h>
#include <string.h>

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
ParsePermission(const char *p)
{
	for (auto i = permission_names; i->name != nullptr; ++i)
		if (strcmp(p, i->name) == 0)
			return i->value;

	throw FormatRuntimeError("unknown permission \"%s\"", p);
}

static unsigned parsePermissions(const char *string)
{
	assert(string != nullptr);

	const char *const end = string + strlen(string);

	unsigned permission = 0;
	while (true) {
		const char *comma = std::find(string, end,
					      PERMISSION_SEPARATOR);
		if (comma > string) {
			const std::string name(string, comma);
			permission |= ParsePermission(name.c_str());
		}

		if (comma == end)
			break;

		string = comma + 1;
	}

	return permission;
}

void
initPermissions(const ConfigData &config)
{
	unsigned permission;

	permission_default = PERMISSION_READ | PERMISSION_ADD |
	    PERMISSION_CONTROL | PERMISSION_ADMIN;

	for (const auto &param : config.GetParamList(ConfigOption::PASSWORD)) {
		permission_default = 0;

		const char *separator = strchr(param.value.c_str(),
					       PERMISSION_PASSWORD_CHAR);

		if (separator == NULL)
			throw FormatRuntimeError("\"%c\" not found in password string "
						 "\"%s\", line %i",
						 PERMISSION_PASSWORD_CHAR,
						 param.value.c_str(),
						 param.line);

		std::string password(param.value.c_str(), separator);

		permission = parsePermissions(separator + 1);

		permission_passwords.insert(std::make_pair(std::move(password),
							   permission));
	}

	const ConfigParam *param;
	param = config.GetParam(ConfigOption::DEFAULT_PERMS);

	if (param)
		permission_default = parsePermissions(param->value.c_str());

#ifdef HAVE_UN
	param = config.GetParam(ConfigOption::LOCAL_PERMISSIONS);
	if (param != nullptr)
		local_permissions = parsePermissions(param->value.c_str());
	else
		local_permissions = permission_default;
#endif
}

int getPermissionFromPassword(char const* password, unsigned* permission)
{
	auto i = permission_passwords.find(password);
	if (i == permission_passwords.end())
		return -1;

	*permission = i->second;
	return 0;
}

unsigned getDefaultPermissions(void)
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
