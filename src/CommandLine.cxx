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
#include "util/Error.hxx"
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
#define CONFIG_FILE_LOCATION		"mpd\\mpd.conf"
#define APP_CONFIG_FILE_LOCATION	"conf\\mpd.conf"
#else
#define USER_CONFIG_FILE_LOCATION1	".mpdconf"
#define USER_CONFIG_FILE_LOCATION2	".mpd/mpd.conf"
#define USER_CONFIG_FILE_LOCATION_XDG	"mpd/mpd.conf"
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
	puts("Music Player Daemon " VERSION
#ifdef GIT_COMMIT
	     " (" GIT_COMMIT ")"
#endif
	     "\n"
	     "\n"
	     "Copyright (C) 2003-2007 Warren Dukes <warren.dukes@gmail.com>\n"
	     "Copyright (C) 2008-2014 Max Kellermann <max@duempel.org>\n"
	     "This is free software; see the source for copying conditions.  There is NO\n"
	     "warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

#ifdef ENABLE_DATABASE
	puts("\n"
	     "Database plugins:");

	for (auto i = database_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);

	puts("\n\n"
	     "Storage plugins:");

	for (auto i = storage_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
	puts("\n\n"
	     "Neighbor plugins:");
	for (auto i = neighbor_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);
#endif

	puts("\n\n"
	     "Decoders plugins:");

	decoder_plugins_for_each([](const DecoderPlugin &plugin){
			printf(" [%s]", plugin.name);

			const char *const*suffixes = plugin.suffixes;
			if (suffixes != nullptr)
				for (; *suffixes != nullptr; ++suffixes)
					printf(" %s", *suffixes);

			puts("");
		});

	puts("\n"
	     "Output plugins:");
	audio_output_plugins_for_each(plugin)
		printf(" %s", plugin->name);
	puts("");

#ifdef ENABLE_ENCODER
	puts("\n"
	     "Encoder plugins:");
	encoder_plugins_for_each(plugin)
		printf(" %s", plugin->name);
	puts("");
#endif

#ifdef ENABLE_ARCHIVE
	puts("\n"
	     "Archive plugins:");
	archive_plugins_for_each(plugin) {
		printf(" [%s]", plugin->name);

		const char *const*suffixes = plugin->suffixes;
		if (suffixes != nullptr)
			for (; *suffixes != nullptr; ++suffixes)
				printf(" %s", *suffixes);

		puts("");
	}
#endif

	puts("\n"
	     "Input plugins:");
	input_plugins_for_each(plugin)
		printf(" %s", plugin->name);

	puts("\n\n"
	     "Playlist plugins:");
	playlist_plugins_for_each(plugin)
		printf(" %s", plugin->name);

	puts("\n\n"
	     "Protocols:");
	print_supported_uri_schemes_to_fp(stdout);

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
	puts("Usage:\n"
	     "  mpd [OPTION...] [path/to/mpd.conf]\n"
	     "\n"
	     "Music Player Daemon - a daemon for playing music.\n"
	     "\n"
	     "Options:");

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
	Error &error;
	bool result;
public:
	ConfigLoader(Error &_error) : error(_error), result(false) { }

	bool GetResult() const { return result; }

	bool TryFile(const Path path);
	bool TryFile(const AllocatedPath &base_path,
		     PathTraitsFS::const_pointer path);
};

bool ConfigLoader::TryFile(Path path)
{
	if (FileExists(path)) {
		result = ReadConfigFile(path, error);
		return true;
	}
	return false;
}

bool ConfigLoader::TryFile(const AllocatedPath &base_path,
			   PathTraitsFS::const_pointer path)
{
	if (base_path.IsNull())
		return false;
	auto full_path = AllocatedPath::Build(base_path, path);
	return TryFile(full_path);
}

bool
parse_cmdline(int argc, char **argv, struct options *options,
	      Error &error)
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

		error.Format(cmdline_domain, "invalid option: %s",
			     parser.GetOption());
		return false;
	}

	/* initialize the logging library, so the configuration file
	   parser can use it already */
	log_early_init(options->verbose);

	if (!use_config_file) {
		LogDebug(cmdline_domain,
			 "Ignoring config, using daemon defaults");
		return true;
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
		error.Set(cmdline_domain, "too many arguments");
		return false;
	}

	if (config_file != nullptr) {
		/* use specified configuration file */
		return ReadConfigFile(Path::FromFS(config_file), error);
	}

	/* use default configuration file path */

	ConfigLoader loader(error);

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
	if (!found) {
		error.Set(cmdline_domain, "No configuration file found");
		return false;
	}

	return loader.GetResult();
}
