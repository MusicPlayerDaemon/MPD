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

AllocatedPath
ParsePath(const char *path)
{
	assert(path != nullptr);

#ifndef _WIN32
	if (path[0] == '~') {
		++path;

		if (*path == '\0')
			return GetConfiguredHome();

		if (*path == '/') {
			++path;

			return GetConfiguredHome() /
				AllocatedPath::FromUTF8Throw(path);
		} else {
			const auto [user, rest] = Split(std::string_view{path}, '/');

			return GetHome(std::string{user}.c_str())
				/ AllocatedPath::FromUTF8Throw(rest);
		}
	} else if (path[0] == '$') {
		++path;

		const auto [env_var, rest] = Split(std::string_view{path}, '/');

	        AllocatedPath xdg_path(nullptr);
		if (env_var == "HOME"sv) {
			xdg_path = GetConfiguredHome();
#ifdef USE_XDG
		} else if (env_var == "XDG_CONFIG_HOME"sv) {
			xdg_path = GetUserConfigDir();
		} else if (env_var == "XDG_MUSIC_DIR"sv) {
			xdg_path = GetUserMusicDir();
		} else if (env_var == "XDG_CACHE_HOME"sv) {
			xdg_path = GetUserCacheDir();
		} else if (env_var == "XDG_RUNTIME_DIR"sv) {
			xdg_path = GetUserRuntimeDir();
#endif
		} else {
			throw FmtRuntimeError("Unknown variable: {:?}", env_var);
		}

		return xdg_path / AllocatedPath::FromUTF8Throw(rest);
	} else if (!PathTraitsUTF8::IsAbsolute(path)) {
		throw FmtRuntimeError("not an absolute path: {:?}", path);
	} else {
#endif
		return AllocatedPath::FromUTF8Throw(path);
#ifndef _WIN32
	}
#endif
}
