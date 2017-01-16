/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "CommandLine.hxx"
#include "ls.hxx"
#include "LogInit.hxx"
#include "Log.hxx"
#include "config/ConfigGlobal.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "output/Registry.hxx"
#include "output/OutputPlugin.hxx"
#include "input/Registry.hxx"
#include "input/InputPlugin.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/PlaylistPlugin.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/StandardDirectory.hxx"
#include "system/Error.hxx"
#include "util/Macros.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/OptionDef.hxx"
#include "util/OptionParser.hxx"

#ifdef ENABLE_DATABASE
#include "db/Registry.hxx"
#include "db/DatabasePlugin.hxx"
#include "storage/Registry.hxx"
#include "storage/StoragePlugin.hxx"
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
#include "neighbor/Registry.hxx"
#include "neighbor/NeighborPlugin.hxx"
#endif

#ifdef ENABLE_ENCODER
#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#endif

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#define CONFIG_FILE_LOCATION PATH_LITERAL("mpd\\mpd.conf")
#define APP_CONFIG_FILE_LOCATION PATH_LITERAL("conf\\mpd.conf")
#else
#define USER_CONFIG_FILE_LOCATION1 PATH_LITERAL(".mpdconf")
#define USER_CONFIG_FILE_LOCATION2 PATH_LITERAL(".mpd/mpd.conf")
#define USER_CONFIG_FILE_LOCATION_XDG PATH_LITERAL("mpd/mpd.conf")
#endif

static constexpr OptionDef opt_kill(
	"kill", "kill the currently running mpd session");
static constexpr OptionDef opt_no_config(
	"no-config", "don't read from config");
static constexpr OptionDef opt_no_daemon(
	"no-daemon", "don't detach from console");
static constexpr OptionDef opt_stdout(
	"stdout", nullptr); // hidden, compatibility with old versions
static constexpr OptionDef opt_stderr(
	"stderr", "print messages to stderr");
static constexpr OptionDef opt_verbose(
	"verbose", 'v', "verbose logging");
static constexpr OptionDef opt_version(
	"version", 'V', "print version number");
static constexpr OptionDef opt_help(
	"help", 'h', "show help options");
static constexpr OptionDef opt_help_alt(
	nullptr, '?', nullptr); // hidden, standard alias for --help

static constexpr Domain cmdline_domain("cmdline");

gcc_noreturn
static void version(void)
{
	printf("Music Player Daemon " VERSION
#ifdef GIT_COMMIT
	       " (" GIT_COMMIT ")"
#endif
	       "\n"
	       "\n"
	       "Copyright (C) 2003-2007 Warren Dukes <warren.dukes@gmail.com>\n"
	       "Copyright 2008-2017 Max Kellermann <max@duempel.org>\n"
	       "This is free software; see the source for copying conditions.  There is NO\n"
	       "warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"

#ifdef ENABLE_DATABASE
	       "\n"
	       "Database plugins:\n");

	for (auto i = database_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);

	printf("\n\n"
	       "Storage plugins:\n");

	for (auto i = storage_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);

	printf("\n"
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
	       "\n"
	       "Neighbor plugins:\n");
	for (auto i = neighbor_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);

	printf("\n"
#endif

	       "\n"
	       "Decoders plugins:\n");

	decoder_plugins_for_each([](const DecoderPlugin &plugin){
			printf(" [%s]", plugin.name);

			const char *const*suffixes = plugin.suffixes;
			if (suffixes != nullptr)
				for (; *suffixes != nullptr; ++suffixes)
					printf(" %s", *suffixes);

			printf("\n");
		});

	printf("\n"
	       "Filters:\n"
#ifdef ENABLE_LIBSAMPLERATE
	       " libsamplerate"
#endif
#ifdef ENABLE_SOXR
	       " soxr"
#endif
	       "\n\n"
	       "Tag plugins:\n"
#ifdef ENABLE_ID3TAG
	       " id3tag"
#endif
	       "\n\n"
	       "Output plugins:\n");
	audio_output_plugins_for_each(plugin)
		printf(" %s", plugin->name);
	printf("\n"

#ifdef ENABLE_ENCODER
	       "\n"
	       "Encoder plugins:\n");
	encoder_plugins_for_each(plugin)
		printf(" %s", plugin->name);
	printf("\n"
#endif

#ifdef ENABLE_ARCHIVE
	       "\n"
	       "Archive plugins:\n");
	archive_plugins_for_each(plugin) {
		printf(" [%s]", plugin->name);

		const char *const*suffixes = plugin->suffixes;
		if (suffixes != nullptr)
			for (; *suffixes != nullptr; ++suffixes)
				printf(" %s", *suffixes);

		printf("\n");
	}

	printf(""
#endif

	       "\n"
	       "Input plugins:\n");
	input_plugins_for_each(plugin)
		printf(" %s", plugin->name);

	printf("\n\n"
	       "Playlist plugins:\n");
	playlist_plugins_for_each(plugin)
		printf(" %s", plugin->name);

	printf("\n\n"
	       "Protocols:\n");
	print_supported_uri_schemes_to_fp(stdout);

	printf("\n"
	       "Other features:\n"
#ifdef HAVE_AVAHI
	       " avahi"
#endif
#ifdef USE_EPOLL
	       " epoll"
#endif
#ifdef HAVE_ICONV
	       " iconv"
#endif
#ifdef HAVE_ICU
	       " icu"
#endif
#ifdef ENABLE_INOTIFY
	       " inotify"
#endif
#ifdef HAVE_IPV6
	       " ipv6"
#endif
#ifdef ENABLE_SYSTEMD_DAEMON
	       " systemd"
#endif
#ifdef HAVE_TCP
	       " tcp"
#endif
#ifdef HAVE_UN
	       " un"
#endif
	       "\n");

	exit(EXIT_SUCCESS);
}

static void PrintOption(const OptionDef &opt)
{
	if (opt.HasShortOption())
		printf("  -%c, --%-12s%s\n",
		       opt.GetShortOption(),
		       opt.GetLongOption(),
		       opt.GetDescription());
	else
		printf("  --%-16s%s\n",
		       opt.GetLongOption(),
		       opt.GetDescription());
}

gcc_noreturn
static void help(void)
{
	printf("Usage:\n"
	       "  mpd [OPTION...] [path/to/mpd.conf]\n"
	       "\n"
	       "Music Player Daemon - a daemon for playing music.\n"
	       "\n"
	       "Options:\n");

	PrintOption(opt_help);
	PrintOption(opt_kill);
	PrintOption(opt_no_config);
	PrintOption(opt_no_daemon);
	PrintOption(opt_stderr);
	PrintOption(opt_verbose);
	PrintOption(opt_version);

	exit(EXIT_SUCCESS);
}

class ConfigLoader
{
public:
	bool TryFile(const Path path);
	bool TryFile(const AllocatedPath &base_path,
		     PathTraitsFS::const_pointer_type path);
};

bool ConfigLoader::TryFile(Path path)
{
	if (FileExists(path)) {
		ReadConfigFile(path);
		return true;
	}
	return false;
}

bool ConfigLoader::TryFile(const AllocatedPath &base_path,
			   PathTraitsFS::const_pointer_type path)
{
	if (base_path.IsNull())
		return false;
	auto full_path = AllocatedPath::Build(base_path, path);
	return TryFile(full_path);
}

void
ParseCommandLine(int argc, char **argv, struct options *options)
{
	bool use_config_file = true;
	options->kill = false;
	options->daemon = true;
	options->log_stderr = false;
	options->verbose = false;

	// First pass: handle command line options
	OptionParser parser(argc, argv);
	while (parser.HasEntries()) {
		if (!parser.ParseNext())
			continue;
		if (parser.CheckOption(opt_kill)) {
			options->kill = true;
			continue;
		}
		if (parser.CheckOption(opt_no_config)) {
			use_config_file = false;
			continue;
		}
		if (parser.CheckOption(opt_no_daemon)) {
			options->daemon = false;
			continue;
		}
		if (parser.CheckOption(opt_stderr, opt_stdout)) {
			options->log_stderr = true;
			continue;
		}
		if (parser.CheckOption(opt_verbose)) {
			options->verbose = true;
			continue;
		}
		if (parser.CheckOption(opt_version))
			version();
		if (parser.CheckOption(opt_help, opt_help_alt))
			help();

		throw FormatRuntimeError("invalid option: %s",
					 parser.GetOption());
	}

	/* initialize the logging library, so the configuration file
	   parser can use it already */
	log_early_init(options->verbose);

	if (!use_config_file) {
		LogDebug(cmdline_domain,
			 "Ignoring config, using daemon defaults");
		return;
	}

	// Second pass: find non-option parameters (i.e. config file)
	const char *config_file = nullptr;
	for (int i = 1; i < argc; ++i) {
		if (OptionParser::IsOption(argv[i]))
			continue;
		if (config_file == nullptr) {
			config_file = argv[i];
			continue;
		}

		throw std::runtime_error("too many arguments");
	}

	if (config_file != nullptr) {
		/* use specified configuration file */
#ifdef _UNICODE
		wchar_t buffer[MAX_PATH];
		auto result = MultiByteToWideChar(CP_ACP, 0, config_file, -1,
						  buffer, ARRAY_SIZE(buffer));
		if (result <= 0)
			throw MakeLastError("MultiByteToWideChar() failed");

		ReadConfigFile(Path::FromFS(buffer));
#else
		ReadConfigFile(Path::FromFS(config_file));
#endif
		return;
	}

	/* use default configuration file path */

	ConfigLoader loader;

	bool found =
#ifdef WIN32
		loader.TryFile(GetUserConfigDir(), CONFIG_FILE_LOCATION) ||
		loader.TryFile(GetSystemConfigDir(), CONFIG_FILE_LOCATION) ||
		loader.TryFile(GetAppBaseDir(), APP_CONFIG_FILE_LOCATION);
#else
		loader.TryFile(GetUserConfigDir(),
			       USER_CONFIG_FILE_LOCATION_XDG) ||
		loader.TryFile(GetHomeDir(), USER_CONFIG_FILE_LOCATION1) ||
		loader.TryFile(GetHomeDir(), USER_CONFIG_FILE_LOCATION2) ||
		loader.TryFile(Path::FromFS(SYSTEM_CONFIG_FILE_LOCATION));
#endif
	if (!found)
		throw std::runtime_error("No configuration file found");
}
