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

#ifdef _WIN32
#undef NOUSER // COM needs the "MSG" typedef, and shlobj.h includes COM headers
#endif

#include "StandardDirectory.hxx"
#include "FileSystem.hxx"
#include "XDG.hxx"
#include "util/StringView.hxx"
#include "config.h"

#include <array>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#endif

#ifdef USE_XDG
#include "util/StringStrip.hxx"
#include "util/StringCompare.hxx"
#include "fs/io/TextFile.hxx"
#include <string.h>
#include <utility>
#endif

#ifdef ANDROID
#include "java/Global.hxx"
#include "android/Environment.hxx"
#include "android/Context.hxx"
#include "Main.hxx"
#endif

#if !defined(_WIN32) && !defined(ANDROID)
class PasswdEntry
{
#if defined(HAVE_GETPWNAM_R) || defined(HAVE_GETPWUID_R)
	std::array<char, 16 * 1024> buf;
	passwd pw;
#endif

	passwd *result{nullptr};
public:
	PasswdEntry() = default;

	bool ReadByName(const char *name) {
#ifdef HAVE_GETPWNAM_R
		getpwnam_r(name, &pw, buf.data(), buf.size(), &result);
#else
		result = getpwnam(name);
#endif
		return result != nullptr;
	}

	bool ReadByUid(uid_t uid) {
#ifdef HAVE_GETPWUID_R
		getpwuid_r(uid, &pw, buf.data(), buf.size(), &result);
#else
		result = getpwuid(uid);
#endif
		return result != nullptr;
	}

	const passwd *operator->() {
		assert(result != nullptr);
		return result;
	}
};
#endif

#ifndef ANDROID
static inline bool
IsValidPathString(PathTraitsFS::const_pointer path)
{
	return path != nullptr && *path != '\0';
}

static inline bool
IsValidDir(PathTraitsFS::const_pointer dir)
{
	return PathTraitsFS::IsAbsolute(dir) &&
	       DirectoryExists(Path::FromFS(dir));
}

static inline AllocatedPath
SafePathFromFS(PathTraitsFS::const_pointer dir)
{
	if (IsValidPathString(dir) && IsValidDir(dir))
		return AllocatedPath::FromFS(dir);
	return nullptr;
}
#endif

#ifdef _WIN32
static AllocatedPath GetStandardDir(int folder_id)
{
	std::array<PathTraitsFS::value_type, MAX_PATH> dir;
	auto ret = SHGetFolderPath(nullptr, folder_id | CSIDL_FLAG_DONT_VERIFY,
				   nullptr, SHGFP_TYPE_CURRENT, dir.data());
	if (FAILED(ret))
		return nullptr;
	return SafePathFromFS(dir.data());
}
#endif

#ifdef USE_XDG

static const char home_prefix[] = "$HOME/";

static bool
ParseConfigLine(char *line, const char *dir_name, AllocatedPath &result_dir)
{
	// strip leading white space
	line = StripLeft(line);

	// check for end-of-line or comment
	if (*line == '\0' || *line == '#')
		return false;

	// check if current setting is for requested dir
	if (!StringStartsWith(line, dir_name))
		return false;
	line += strlen(dir_name);

	// strip equals sign and spaces around it
	line = StripLeft(line);
	if (*line != '=')
		return false;
	++line;
	line = StripLeft(line);

	// check if path is quoted
	bool quoted = false;
	if (*line == '"') {
		++line;
		quoted = true;
	}

	// check if path is relative to $HOME
	bool home_relative = false;
	if (StringStartsWith(line, home_prefix)) {
		line += strlen(home_prefix);
		home_relative = true;
	}


	char *line_end;
	// find end of the string
	if (quoted) {
		line_end = std::strrchr(line, '"');
		if (line_end == nullptr)
			return true;
	} else {
		line_end = StripRight(line, line + strlen(line));
	}

	// check for empty result
	if (line == line_end)
		return true;

	*line_end = 0;

	// build the result path
	const auto path_fs = Path::FromFS(line);

	AllocatedPath result = nullptr;
	if (home_relative) {
		auto home = GetHomeDir();
		if (home.IsNull())
			return true;
		result = home / path_fs;
	} else {
		result = AllocatedPath(path_fs);
	}

	if (IsValidDir(result.c_str())) {
		result_dir = std::move(result);
		return true;
	}
	return true;
}

static AllocatedPath
GetUserDir(const char *name) noexcept
try {
	AllocatedPath result = nullptr;
	auto config_dir = GetUserConfigDir();
	if (config_dir.IsNull())
		return result;

	TextFile input(config_dir / Path::FromFS("user-dirs.dirs"));
	char *line;
	while ((line = input.ReadLine()) != nullptr)
		if (ParseConfigLine(line, name, result))
			return result;
	return result;
} catch (const std::exception &e) {
	return nullptr;
}

#endif

AllocatedPath
GetUserConfigDir() noexcept
{
#if defined(_WIN32)
	return GetStandardDir(CSIDL_LOCAL_APPDATA);
#elif defined(USE_XDG)
	// Check for $XDG_CONFIG_HOME
	if (const auto config_home = getenv("XDG_CONFIG_HOME");
	    IsValidPathString(config_home) && IsValidDir(config_home))
		return AllocatedPath::FromFS(config_home);

	// Check for $HOME/.config
	if (const auto home = GetHomeDir(); !home.IsNull()) {
		auto fallback = home / Path::FromFS(".config");
		if (IsValidDir(fallback.c_str()))
			return fallback;
	}

	return nullptr;
#else
	return nullptr;
#endif
}

AllocatedPath
GetUserMusicDir() noexcept
{
#if defined(_WIN32)
	return GetStandardDir(CSIDL_MYMUSIC);	
#elif defined(USE_XDG)
	return GetUserDir("XDG_MUSIC_DIR");
#elif defined(ANDROID)
	return Environment::getExternalStoragePublicDirectory("Music");
#else
	return nullptr;
#endif
}

AllocatedPath
GetUserCacheDir() noexcept
{
#ifdef USE_XDG
	// Check for $XDG_CACHE_HOME
	if (const auto cache_home = getenv("XDG_CACHE_HOME");
	    IsValidPathString(cache_home) && IsValidDir(cache_home))
		return AllocatedPath::FromFS(cache_home);

	// Check for $HOME/.cache
	if (const auto home = GetHomeDir(); !home.IsNull())
		if (auto fallback = home / Path::FromFS(".cache");
		    IsValidDir(fallback.c_str()))
			return fallback;

	return nullptr;
#elif defined(ANDROID)
	return context->GetCacheDir(Java::GetEnv());
#else
	return nullptr;
#endif
}

AllocatedPath
GetUserRuntimeDir() noexcept
{
#ifdef USE_XDG
	return SafePathFromFS(getenv("XDG_RUNTIME_DIR"));
#else
	return nullptr;
#endif
}

AllocatedPath
GetAppRuntimeDir() noexcept
{
#ifdef __linux__
	/* systemd specific; see systemd.exec(5) */
	if (const char *runtime_directory = getenv("RUNTIME_DIRECTORY"))
		if (auto dir = StringView{runtime_directory}.Split(':').first;
		    !dir.empty())
			return AllocatedPath::FromFS(dir);
#endif

#ifdef USE_XDG
	if (const auto user_dir = GetUserRuntimeDir(); !user_dir.IsNull()) {
		auto dir = user_dir / Path::FromFS("mpd");
		mkdir(dir.c_str(), 0700);
		return dir;
	}
#endif

	return nullptr;
}

#ifdef _WIN32

AllocatedPath
GetSystemConfigDir() noexcept
{
	return GetStandardDir(CSIDL_COMMON_APPDATA);
}

AllocatedPath
GetAppBaseDir() noexcept
{
	std::array<PathTraitsFS::value_type, MAX_PATH> app;
	auto ret = GetModuleFileName(nullptr, app.data(), app.size());

	// Check for error
	if (ret == 0)
		return nullptr;

	// Check for truncation
	if (ret == app.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		return nullptr;

	auto app_path = AllocatedPath::FromFS(PathTraitsFS::string_view(app.data(), ret));
	return app_path.GetDirectoryName().GetDirectoryName();
}

#else

AllocatedPath
GetHomeDir() noexcept
{
#ifndef ANDROID
	if (const auto home = getenv("HOME");
	    IsValidPathString(home) && IsValidDir(home))
		return AllocatedPath::FromFS(home);

	if (PasswdEntry pw; pw.ReadByUid(getuid()))
		return SafePathFromFS(pw->pw_dir);
#endif
	return nullptr;
}

AllocatedPath
GetHomeDir(const char *user_name) noexcept
{
#ifdef ANDROID
	(void)user_name;
#else
	assert(user_name != nullptr);

	if (PasswdEntry pw; pw.ReadByName(user_name))
		return SafePathFromFS(pw->pw_dir);
#endif
	return nullptr;
}

#endif
