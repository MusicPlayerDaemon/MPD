// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Permission.hxx"
#include "config/Param.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringSplit.hxx"

#ifdef HAVE_TCP
#include "net/AddressInfo.hxx"
#include "net/MaskedInetAddress.hxx"
#include "net/Resolver.hxx"
#include "net/ToString.hxx"
#endif

#include <cassert>
#include <forward_list>
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
struct HostPermissions {
	MaskedInetAddress address;

	unsigned permissions;
};

static std::forward_list<HostPermissions> host_permissions;
#endif

static unsigned
ParsePermission(std::string_view s)
{
	for (auto i = permission_names; i->name != nullptr; ++i)
		if (s == i->name)
			return i->value;

	throw FmtRuntimeError("unknown permission {:?}", s);
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
				throw FmtRuntimeError("{:?} not found in password string",
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

			MaskedInetAddress masked_address;
			if (masked_address.Parse(host_s.c_str())) {
				host_permissions.emplace_front(HostPermissions{masked_address, permissions});
			} else {
				for (const auto &i : Resolve(host_s.c_str(), 0,
							     AI_PASSIVE, SOCK_STREAM)) {
					if (masked_address.CopyFrom(i, 0))
						host_permissions.emplace_front(masked_address, permissions);
				}
			}
		});
	}
#endif
}

#ifdef HAVE_TCP

int
GetPermissionsFromAddress(SocketAddress _address) noexcept
{
	BareInetAddress address;
	if (!address.CopyFrom(_address))
		return -1;

	for (const auto &i : host_permissions)
		if (i.address.Matches(address))
			return i.permissions;

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
