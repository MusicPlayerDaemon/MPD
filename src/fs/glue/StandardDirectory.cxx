// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifdef _WIN32
#undef NOUSER // COM needs the "MSG" typedef, and shlobj.h includes COM headers
#endif

#include "StandardDirectory.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/XDG.hxx"
#include "config.h"
#include "util/StringSplit.hxx"

#include <array>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <stdlib.h>
#include <pwd.h>
#endif

#ifdef USE_XDG
#include "util/StringSplit.hxx"
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

#ifdef USE_XDG
#include "Version.h" // for PACKAGE_NAME
#define APP_FILENAME PATH_LITERAL(PACKAGE_NAME)
static constexpr Path app_filename = Path::FromFS(APP_FILENAME);
#endif

using std::string_view_literals::operator""sv;

#if !defined(_WIN32) && !defined(ANDROID)
class PasswdEntry
{
#if defined(HAVE_GETPWNAM_R) || defined(HAVE_GETPWUID_R)
	std::array<char, 16 * 1024> buf;
	passwd pw;
#endif

	passwd *result{nullptr};
public:
	PasswdEntry() noexcept = default;

	bool ReadByName(const char *name) noexcept {
#ifdef HAVE_GETPWNAM_R
		getpwnam_r(name, &pw, buf.data(), buf.size(), &result);
#else
		result = getpwnam(name);
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

[[gnu::pure]]
static inline bool
IsValidPathString(PathTraitsFS::const_pointer path) noexcept
{
	return path != nullptr && *path != '\0';
}

[[gnu::pure]]
static inline bool
IsValidDir(Path path) noexcept
{
	return path.IsAbsolute() && DirectoryExists(path);
}

[[gnu::pure]]
static inline AllocatedPath
SafePathFromFS(PathTraitsFS::const_pointer dir) noexcept
{
	if (!IsValidPathString(dir))
		return nullptr;

	if (const Path path = Path::FromFS(dir); IsValidDir(path))
		return AllocatedPath{path};

	return nullptr;
}
#endif

#ifdef _WIN32

[[gnu::pure]]
static AllocatedPath
GetStandardDir(int folder_id) noexcept
{
	std::array<PathTraitsFS::value_type, MAX_PATH> dir;
	auto ret = SHGetFolderPath(nullptr, folder_id | CSIDL_FLAG_DONT_VERIFY,
				   nullptr, SHGFP_TYPE_CURRENT, dir.data());
	if (FAILED(ret))
		return nullptr;
	return SafePathFromFS(dir.data());
}

#endif

#if !defined(_WIN32) && !defined(ANDROID)

[[gnu::pure]]
static Path
GetEnvPath(const char *name) noexcept
{
	if (const char *value = getenv(name); IsValidPathString(value))
		return Path::FromFS(value);

	return nullptr;
}

[[gnu::pure]]
static Path
GetAbsoluteEnvPath(const char *name) noexcept
{
	if (const auto path = GetEnvPath(name);
	    path != nullptr && path.IsAbsolute())
		return path;

	return nullptr;
}

[[gnu::pure]]
static Path
GetExistingEnvDirectory(const char *name) noexcept
{
	if (const auto path = GetAbsoluteEnvPath(name);
	    path != nullptr && DirectoryExists(path))
		return path;

	return nullptr;
}

#endif

#ifdef USE_XDG

static bool
ParseConfigLine(std::string_view line, std::string_view dir_name,
		AllocatedPath &result_dir) noexcept
{
	// strip leading white space
	line = StripLeft(line);

	// check for end-of-line or comment
	if (line.empty() || line.front() == '#')
		return false;

	// check if current setting is for requested dir
	if (!SkipPrefix(line, dir_name))
		return false;

	// strip equals sign and spaces around it
	line = StripLeft(line);
	if (!SkipPrefix(line, "="sv))
		return false;
	line = StripLeft(line);

	if (line.empty())
		return true;

	// check if path is quoted
	const bool quoted = SkipPrefix(line, "\""sv);

	// check if path is relative to $HOME
	const bool home_relative = SkipPrefix(line, "$HOME"sv);

	// find end of the string
	std::string_view path_view;
	if (quoted) {
		const auto [pv, rest] = SplitLast(line, '"');
		if (rest.data() == nullptr)
			return true;

		path_view = pv;
	} else {
		path_view = line;
		path_view = StripRight(path_view);
	}

	// check for empty result
	if (path_view.empty())
		return true;

	// build the result path
	auto result = AllocatedPath::FromFS(path_view);

	if (home_relative) {
		auto home = GetHomeDir();
		if (home.IsNull())
			return true;
		result = home / result;
	}

	if (IsValidDir(result)) {
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
	if (const auto path = GetExistingEnvDirectory("XDG_CONFIG_HOME");
	    path != nullptr)
		return AllocatedPath{path};

	// Check for $HOME/.config
	if (const auto home = GetHomeDir(); !home.IsNull()) {
		auto fallback = home / Path::FromFS(".config");
		if (IsValidDir(fallback))
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
	return Environment::getExternalStoragePublicDirectory(Java::GetEnv(),
							      "Music");
#else
	return nullptr;
#endif
}

AllocatedPath
GetUserCacheDir() noexcept
{
#ifdef USE_XDG
	// Check for $XDG_CACHE_HOME
	if (const auto path = GetExistingEnvDirectory("XDG_CACHE_HOME");
	    path != nullptr)
		return AllocatedPath{path};

	// Check for $HOME/.cache
	if (const auto home = GetHomeDir(); !home.IsNull())
		if (auto fallback = home / Path::FromFS(".cache");
		    IsValidDir(fallback))
			return fallback;

	return nullptr;
#elif defined(ANDROID)
	return context->GetCacheDir(Java::GetEnv());
#else
	return nullptr;
#endif
}

AllocatedPath
GetAppCacheDir() noexcept
{
#ifdef USE_XDG
	if (const auto user_dir = GetUserCacheDir(); !user_dir.IsNull()) {
		auto dir = user_dir / app_filename;
		CreateDirectoryNoThrow(dir);
		return dir;
	}

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
	if (const auto path = GetExistingEnvDirectory("XDG_RUNTIME_DIR");
	    path != nullptr)
		return AllocatedPath{path};
#endif

	return nullptr;
}

AllocatedPath
GetAppRuntimeDir() noexcept
{
#if defined(__linux__) && !defined(ANDROID)
	/* systemd specific; see systemd.exec(5) */
	if (const char *runtime_directory = getenv("RUNTIME_DIRECTORY"))
		if (auto dir = Split(std::string_view{runtime_directory}, ':').first;
		    !dir.empty())
			return AllocatedPath::FromFS(dir);
#endif

#ifdef USE_XDG
	if (const auto user_dir = GetUserRuntimeDir(); !user_dir.IsNull()) {
		auto dir = user_dir / app_filename;
		CreateDirectoryNoThrow(dir);
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
	if (const auto home = GetExistingEnvDirectory("HOME"); home != nullptr)
		return AllocatedPath{home};
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
