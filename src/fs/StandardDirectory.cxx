/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

// Use X Desktop guidelines where applicable
#if !defined(__APPLE__) && !defined(WIN32) && !defined(ANDROID)
#define USE_XDG
#endif

#include "StandardDirectory.hxx"
#include "FileSystem.hxx"

#include <array>

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#endif

#ifdef USE_XDG
#include "util/Error.hxx"
#include "util/StringUtil.hxx"
#include "io/TextFile.hxx"
#include <string.h>
#include <utility>
#endif

#ifdef ANDROID
#include "java/Global.hxx"
#include "android/Environment.hxx"
#include "android/Context.hxx"
#include "Main.hxx"
#endif

#ifndef WIN32
class PasswdEntry
{
#if defined(HAVE_GETPWNAM_R) || defined(HAVE_GETPWUID_R)
	std::array<char, 16 * 1024> buf;
	passwd pw;
#endif

	passwd *result;
public:
	PasswdEntry() : result(nullptr) { }

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

static inline bool IsValidPathString(PathTraitsFS::const_pointer path)
{
	return path != nullptr && *path != '\0';
}

static inline bool IsValidDir(PathTraitsFS::const_pointer dir)
{
	return PathTraitsFS::IsAbsolute(dir) &&
	       DirectoryExists(Path::FromFS(dir));
}

static inline AllocatedPath SafePathFromFS(PathTraitsFS::const_pointer dir)
{
	if (IsValidPathString(dir) && IsValidDir(dir))
		return AllocatedPath::FromFS(dir);
	return AllocatedPath::Null();
}

#ifdef WIN32
static AllocatedPath GetStandardDir(int folder_id)
{
	std::array<char, MAX_PATH> dir;
	auto ret = SHGetFolderPath(nullptr, folder_id | CSIDL_FLAG_DONT_VERIFY,
				   nullptr, SHGFP_TYPE_CURRENT, dir.data());
	if (FAILED(ret))
		return AllocatedPath::Null();
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
		line_end = strrchr(line, '"');
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
	const char *path = line;

	auto result = AllocatedPath::Null();
	if (home_relative) {
		auto home = GetHomeDir();
		if (home.IsNull())
			return true;
		result = AllocatedPath::Build(home, path);
	} else {
		result = AllocatedPath::FromFS(path);
	}

	if (IsValidDir(result.c_str())) {
		result_dir = std::move(result);
		return true;
	}
	return true;
}

static AllocatedPath GetUserDir(const char *name)
{
	auto result = AllocatedPath::Null();
	auto config_dir = GetUserConfigDir();
	if (config_dir.IsNull())
		return result;
	auto dirs_file = AllocatedPath::Build(config_dir, "user-dirs.dirs");
	TextFile input(dirs_file, IgnoreError());
	if (input.HasFailed())
		return result;
	char *line;
	while ((line = input.ReadLine()) != nullptr)
		if (ParseConfigLine(line, name, result))
			return result;
	return result;
}

#endif

AllocatedPath GetUserConfigDir()
{
#if defined(WIN32)
	return GetStandardDir(CSIDL_LOCAL_APPDATA);
#elif defined(USE_XDG)
	// Check for $XDG_CONFIG_HOME
	auto config_home = getenv("XDG_CONFIG_HOME");
	if (IsValidPathString(config_home) && IsValidDir(config_home))
		return AllocatedPath::FromFS(config_home);

	// Check for $HOME/.config
	auto home = GetHomeDir();
	if (!home.IsNull()) {
		AllocatedPath fallback = AllocatedPath::Build(home, ".config");
		if (IsValidDir(fallback.c_str()))
			return fallback;
	}

	return AllocatedPath::Null();
#else
	return AllocatedPath::Null();
#endif
}

AllocatedPath GetUserMusicDir()
{
#if defined(WIN32)
	return GetStandardDir(CSIDL_MYMUSIC);	
#elif defined(USE_XDG)
	return GetUserDir("XDG_MUSIC_DIR");
#elif defined(ANDROID)
	return Environment::getExternalStoragePublicDirectory("Music");
#else
	return AllocatedPath::Null();
#endif
}

AllocatedPath GetUserCacheDir()
{
#ifdef USE_XDG
	// Check for $XDG_CACHE_HOME
	auto cache_home = getenv("XDG_CACHE_HOME");
	if (IsValidPathString(cache_home) && IsValidDir(cache_home))
		return AllocatedPath::FromFS(cache_home);

	// Check for $HOME/.cache
	auto home = GetHomeDir();
	if (!home.IsNull()) {
		AllocatedPath fallback = AllocatedPath::Build(home, ".cache");
		if (IsValidDir(fallback.c_str()))
			return fallback;
	}

	return AllocatedPath::Null();
#elif defined(ANDROID)
	return context->GetCacheDir(Java::GetEnv());
#else
	return AllocatedPath::Null();
#endif
}

#ifdef WIN32

AllocatedPath GetSystemConfigDir()
{
	return GetStandardDir(CSIDL_COMMON_APPDATA);
}

AllocatedPath GetAppBaseDir()
{
	std::array<char, MAX_PATH> app;
	auto ret = GetModuleFileName(nullptr, app.data(), app.size());

	// Check for error
	if (ret == 0)
		return AllocatedPath::Null();

	// Check for truncation
	if (ret == app.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		return AllocatedPath::Null();

	auto app_path = AllocatedPath::FromFS(app.data());
	return app_path.GetDirectoryName().GetDirectoryName();
}

#else

AllocatedPath GetHomeDir()
{
	auto home = getenv("HOME");
	if (IsValidPathString(home) && IsValidDir(home))
		return AllocatedPath::FromFS(home);
	PasswdEntry pw;
	if (pw.ReadByUid(getuid()))
		return SafePathFromFS(pw->pw_dir);
	return AllocatedPath::Null();
}

AllocatedPath GetHomeDir(const char *user_name)
{
	assert(user_name != nullptr);
	PasswdEntry pw;
	if (pw.ReadByName(user_name))
		return SafePathFromFS(pw->pw_dir);
	return AllocatedPath::Null();
}

#endif
