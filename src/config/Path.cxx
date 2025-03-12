// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Path.hxx"
#include "Data.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/XDG.hxx"
#include "fs/glue/StandardDirectory.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringSplit.hxx"

#include <cassert>

using std::string_view_literals::operator""sv;

#ifndef _WIN32
#include <pwd.h>

static const char *configured_user = nullptr;

/**
 * Determine a given user's home directory.
 */
static AllocatedPath
GetHome(const char *user)
{
	AllocatedPath result = GetHomeDir(user);
	if (result.IsNull())
		throw FmtRuntimeError("no such user: {:?}", user);

	return result;
}

/**
 * Determine the current user's home directory.
 */
static AllocatedPath
GetHome()
{
	AllocatedPath result = GetHomeDir();
	if (result.IsNull())
		throw std::runtime_error("problems getting home for current user");

	return result;
}

/**
 * Determine the configured user's home directory.
 *
 * Throws #std::runtime_error on error.
 */
static AllocatedPath
GetConfiguredHome()
{
	return configured_user != nullptr
		? GetHome(configured_user)
		: GetHome();
}

#endif

void
InitPathParser(const ConfigData &config) noexcept
{
#ifdef _WIN32
	(void)config;
#else
	configured_user = config.GetString(ConfigOption::USER);
#endif
}

#ifndef _WIN32

static AllocatedPath
GetVariable(std::string_view name)
{
	if (name == "HOME"sv)
		return GetConfiguredHome();
	else if (name == "XDG_CONFIG_HOME"sv)
		return GetUserConfigDir();
	else if (name == "XDG_MUSIC_DIR"sv)
		return GetUserMusicDir();
	else if (name == "XDG_CACHE_HOME"sv)
		return GetUserCacheDir();
	else if (name == "XDG_RUNTIME_DIR"sv)
		return GetUserRuntimeDir();
	else
		throw FmtRuntimeError("Unknown variable: {:?}", name);
}

#endif

AllocatedPath
ParsePath(std::string_view path)
{
#ifndef _WIN32
	if (path.starts_with('~')) {
		path.remove_prefix(1);

		if (path.empty())
			return GetConfiguredHome();

		const auto [user, rest] = Split(path, '/');
		const auto home = user.empty()
			? GetConfiguredHome()
			: GetHome(std::string{user}.c_str());

		return home / AllocatedPath::FromUTF8Throw(rest);
	} else if (path.starts_with('$')) {
		path.remove_prefix(1);

		const auto [name, rest] = Split(path, '/');
		const auto value = GetVariable(name);
		if (value.IsNull())
			throw FmtRuntimeError("No value for variable: {:?}", name);

		return value / AllocatedPath::FromUTF8Throw(rest);
	} else if (!PathTraitsUTF8::IsAbsolute(path)) {
		throw FmtRuntimeError("not an absolute path: {:?}", path);
	} else {
#endif
		return AllocatedPath::FromUTF8Throw(path);
#ifndef _WIN32
	}
#endif
}
