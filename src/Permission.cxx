// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Permission.hxx"
#include "config/Param.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "net/AddressInfo.hxx"
#include "net/Resolver.hxx"
#include "net/ToString.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringSplit.hxx"

#include <cassert>
#include <map>
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
	{ "player", PERMISSION_PLAYER },
	{ "control", PERMISSION_CONTROL },
	{ "admin", PERMISSION_ADMIN },
	{ nullptr, 0 },
};

static std::map<std::string, unsigned, std::less<>> permission_passwords;

static unsigned permission_default;

#ifdef HAVE_UN
static unsigned local_permissions;
#endif

#ifdef HAVE_TCP
static std::map<std::string, unsigned, std::less<>> host_passwords;
#endif

static unsigned
ParsePermission(std::string_view s)
{
	for (auto i = permission_names; i->name != nullptr; ++i)
		if (s == i->name)
			return i->value;

	throw FmtRuntimeError("unknown permission \"{}\"", s);
}

static unsigned
parsePermissions(std::string_view string)
{
	unsigned permission = 0;

	for (const auto i : IterableSplitString(string, PERMISSION_SEPARATOR))
		if (!i.empty())
			permission |= ParsePermission(i);

	/* for backwards compatiblity with MPD 0.22 and older,
	   "control" implies "play" */
	if (permission & PERMISSION_CONTROL)
		permission |= PERMISSION_PLAYER;

	return permission;
}

void
initPermissions(const ConfigData &config)
{
	permission_default = PERMISSION_READ | PERMISSION_ADD |
		PERMISSION_PLAYER |
	    PERMISSION_CONTROL | PERMISSION_ADMIN;

	for (const auto &param : config.GetParamList(ConfigOption::PASSWORD)) {
		permission_default = 0;

		param.With([](const std::string_view value){
			const auto [password, permissions] =
				Split(value, PERMISSION_PASSWORD_CHAR);
			if (permissions.data() == nullptr)
				throw FmtRuntimeError("\"{}\" not found in password string",
						      PERMISSION_PASSWORD_CHAR);

			permission_passwords.emplace(password,
						     parsePermissions(permissions));
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

#ifdef HAVE_TCP
	for (const auto &param : config.GetParamList(ConfigOption::HOST_PERMISSIONS)) {
		permission_default = 0;

		param.With([](std::string_view value){
			auto [host_sv, permissions_s] = Split(value, ' ');
			unsigned permissions = parsePermissions(permissions_s);

			const std::string host_s{host_sv};

			for (const auto &i : Resolve(host_s.c_str(), 0,
						     AI_PASSIVE, SOCK_STREAM))
				host_passwords.emplace(HostToString(i),
						       permissions);
		});
	}
#endif
}

#ifdef HAVE_TCP

int
GetPermissionsFromAddress(SocketAddress address) noexcept
{
	if (auto i = host_passwords.find(HostToString(address));
	    i != host_passwords.end())
		return i->second;

	return -1;
}

#endif

std::optional<unsigned>
GetPermissionFromPassword(const char *password) noexcept
{
	auto i = permission_passwords.find(password);
	if (i == permission_passwords.end())
		return std::nullopt;

	return i->second;
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
